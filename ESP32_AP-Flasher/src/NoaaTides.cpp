#ifndef WITHOUT_NOAA_TIDES
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <FS.h>
#include <DrawOWM.h>

#include "tag_db.h"
#include "makeimage.h"
#include "TFT_eSPI.h"
#include "contentmanager.h"
#include "web.h"
#include "storage.h"
#include "util.h"
#include "NoaaTides.h"

#define ENABLE_LOGGING  1
#if ENABLE_LOGGING && __has_include("logging.h") 
#include "logging.h"
#else
#define LOG(format, ...)
#define LOG_RAW(format, ...)
#endif

#define VERBOSE_LOGGING 0
#if ENABLE_LOGGING && VERBOSE_LOGGING
#define VLOG(format, ...) _LOG(format,## __VA_ARGS__))
#else
#define VLOG(format, ...)
#endif

// #define DEBUG_STARTTIME "8/30/26 08:22:01"
#ifdef DEBUG_STARTTIME
void SetDebugStartTime(time_t *pStartTime)
{
   struct tm Timeinfo;
   const char *DeBugTime = DEBUG_STARTTIME;
   strptime(DeBugTime,"%m/%d/%y %H:%M:%S",&Timeinfo);
   Timeinfo.tm_isdst = 1;
   *pStartTime = mktime(&Timeinfo);
   LOG("Start time %d %s",*pStartTime,ctime(pStartTime));
}
#else
#define SetDebugStartTime(x)
#endif


static time_t Time2StartOfHour(time_t Time)
{
   struct tm Timeinfo;

   localtime_r(&Time,&Timeinfo);
   Timeinfo.tm_min = 0;
   return mktime(&Timeinfo);
}

//   String StationID = "8446613"; // Wellfleet
//   String StationID = "9415009"; // San Pedro

// Return tide predications for 24 hours starting at the specified time including
// one tide event from before the start time and once after the 24 hour 
// period.
int NoaaTides::GetNoaaTides(String StationID,time_t Start,time_t Duration)
{
   time_t Time;
   time_t EndTime;
   time_t TideTime;
   struct tm Timeinfo;
   struct tm TimeinfoToday;
   char BeginDate[10];
   char EndDate[10];  // for example "20240605"
   JsonDocument doc;
   JsonDocument loc;
   int Yesterday = 0;
   int Today = 0;
   int Entries = 0;
   int Day;
   float Height;
   int Hrs;
   int Mins;
   String DateTime;
   String TideType;
   int i;
   HighLowArray_t Values;
   HighLowArray_t FirstTideValues;

   SetDebugStartTime(&Start);
// The graph starts on the hour, round Start down to the start of the hour
   Start = Time2StartOfHour(Start);

   Time = Start;
   LOG("Start time %s",ctime(&Time));
   localtime_r(&Time,&Timeinfo);
   Day = Timeinfo.tm_mday;
   LOG("Set Today to %d\n",Day);

// Begin predictions 24 hours ago, ending after 48 hours
// This ensures we get at leaset one event before the time window and one after
   EndTime = Time + 2*(24*60*60);
   Time -= 24*60*60;
   localtime_r(&Time,&Timeinfo);
   snprintf(BeginDate,sizeof(BeginDate),"%d%02d%02d",
            Timeinfo.tm_year + 1900,Timeinfo.tm_mon+1,Timeinfo.tm_mday);

   localtime_r(&EndTime,&Timeinfo);
   snprintf(EndDate,sizeof(EndDate),"%d%02d%02d",
            Timeinfo.tm_year + 1900,Timeinfo.tm_mon+1,Timeinfo.tm_mday);

   String TideUrl = "https://api.tidesandcurrents.noaa.gov/"
                    "api/prod/datagetter?begin_date=";
   TideUrl += BeginDate;
   TideUrl += "&end_date=";
   TideUrl += EndDate;
   TideUrl += "&station=";
   TideUrl += StationID;
   TideUrl += "&product=predictions&datum=MLLW"
              "&time_zone=lst_ldt&units=english"
              "&format=json&interval=hilo";

   LOG("TideUrl: %s\n",TideUrl.c_str());

   const bool success = util::httpGetJson(TideUrl,doc, 5000);
   if(!success) {
      LOG("httpGetJson() failed\n");
      return -1;
   }

   EndTime = Start + Duration;
   JsonArray Predictions = doc["predictions"];
   for(JsonObject Prediction : Predictions) {
      DateTime = Prediction["t"].as<String>();
      Height = Prediction["v"].as<float>();
      TideType = Prediction["type"].as<String>();

      Serial.println("{ t: " + DateTime
                     + ", v: " + Height
                     + ", type: " + (Prediction["type"].as<String>())
                     + "}");
      if(sscanf(DateTime.c_str(),"%*d-%*d-%d %d:%d",&Day,&Hrs,&Mins) != 3) {
         Serial.println("Couldn't convert " + DateTime);
         break;
      }

      strptime(DateTime.c_str(),"%F %R",&Timeinfo);
      TideTime = mktime(&Timeinfo);
      LOG("Tide time: %d-%d-%d %d:%02d\n",
          Timeinfo.tm_year + 1900,Timeinfo.tm_mon+1,Timeinfo.tm_mday,
          Timeinfo.tm_hour,Timeinfo.tm_min);

      LOG("Day %d Hrs %d Mins %d\n",Day,Hrs,Mins);

      Values.Height = Height;
      Values.Time = TideTime;

      if(TideType == "H") {
         Values.LowTide = false;
      }
      else if(TideType == "L") {
         Values.LowTide = true;
      }
      else {
         ELOG("Unknown tide type %s\n",TideType.c_str());
         break;
      }

      if(TideTime < Start) {
      // Save for now, we only return one tide before the start time
         FirstTideValues = Values;
      }
      else if(TideTime > EndTime) {
      // We're done
         Tides.push_back(Values);
         Entries++;
         break;
      }
      else {
      // Tide is in range
         if(Entries == 0) {
         // Save the last tide before the start
            Tides.push_back(FirstTideValues);
            Entries++;
         }
         Tides.push_back(Values);
         Entries++;
      }
   }

   LOG("Returning %d entries\n",Tides.size());

   for(i = 0; i < Tides.size(); i++) {
      LOG("%d: %f foot %s tide @ %d %s",
          i,
          Tides[i].Height,
          Tides[i].LowTide ? "low" : "high",
          Tides[i].Time,
          ctime(&Tides[i].Time));
   }

   return Entries;
}

// #define TIDE_PLOT_COLOR TFT_YELLOW
#define TIDE_PLOT_COLOR TFT_BLACK
int NoaaTides::PlotNoaaTides(class DrawOWM * &owm,time_t Start,time_t Duration)
{
   float Height;
   float MinHeight = 1000.0;
   float MaxHeight = -1000.0;
   int x0,x1,y0,y1;
   int i;
   struct tm Timeinfo;
   int Entries = Tides.size();

   SetDebugStartTime(&Start);

// The graph starts on the hour, round Start down to the start of the hour
   Start = Time2StartOfHour(Start);

   LOG("Start %d Duration %d Entries %d\n",Start,Duration,Entries);
   owm->GetBB(BB_GRAPH_DATA,x0,x1,y0,y1);

// Update Min/Max height for midnight
   std::vector<bezier::Vec2> BezierPoints;
	bezier::Bezier<3> cubicBezier;
	bezier::Vec2 ThisPoint;
   double MidwayTime;
   double SegmentTime;

   for(i = 1; i < Entries; i++) {
      InitSegment(Start,i,cubicBezier,SegmentTime);
      ThisPoint = cubicBezier.valueAt(0.0);
      double Height = ThisPoint.y;

      if(MinHeight > Height) {
         MinHeight = Height;
      }
      if(MaxHeight < Height) {
         MaxHeight = Height;
      }
		LOG("%d: SegmentTime %f height %f\n",i,SegmentTime,Height);
      BezierPoints.clear();
   }

   LOG("MinHeight %f\n",MinHeight);
   LOG("MaxHeight %f\n",MaxHeight);

   uint16_t GraphLeft = x0;
   uint16_t GraphTop = y0;
   uint16_t GraphBottom = y1;
	uint16_t GraphWidth = x1 - x0;
   uint16_t DrawX = x0; // right
   uint16_t DrawY = y1; // bottom

   LOG("Graph %d x %d @ %d,%d\n",GraphWidth,y1-GraphTop,x0,y0);
   LOG("GraphTop %d GraphBottom %d\n",GraphTop,GraphBottom);

// Leave room for tide max/min height labels
   owm->setFreeFont(owm->LabelFont);
   String TideHeight = String(MaxHeight,1);
   uint16_t ValueHeight = owm->getStringHeight(TideHeight);
   GraphBottom -= ValueHeight + 4;
   GraphTop += ValueHeight + 4;
   LOG("ValueHeight %d GraphTop %d GraphBottom %d\n",ValueHeight,GraphTop,GraphBottom);

	bool bFirst = true;
	uint16_t LastY = 0;
	i = 0;

   for(int j = 0; j < GraphWidth; j++) {
   // Start at the last high/low tide
		double t = (double) j / GraphWidth;
		int Time = Start + (t * Duration);

		if(bFirst || Time >= Tides[i+1].Time) {
			if(bFirst) {
				LOG("First Segment\n");
				bFirst = false;
			}
			else {
			// move to next segment
				i++;
            if(i > Entries - 2) {
               ELOG("Invalid index %d\n",i);
               return 1;
            }
				LOG("segment %d %d > %d\n",i,Time,Tides[i].Time);
				BezierPoints.clear();
			}
         InitSegment(Start,i,cubicBezier,SegmentTime);
         if(i > 0) {
            int x = GraphLeft + GraphWidth * (Tides[i].Time - Start) / Duration;
            Height = Tides[i].Height;
            DrawY = GraphBottom;
            DrawY -= ((Height - MinHeight) / (MaxHeight - MinHeight)) *
                    (GraphBottom - GraphTop);
            if(Tides[i].LowTide) {
            // Display low tide height below graph
               DrawY += ValueHeight;
            }
            else {
            // Display high tide height above graph
               DrawY -= 4;
            }
         // label 
            TideHeight = String(Height,2) + " ft";
            uint16_t HeighWidth = owm->getStringWidth(TideHeight);
            uint16_t xTideHeight = x - (HeighWidth / 2);
            if(xTideHeight < GraphLeft) {
               LOG("Height label moved right to fit\n");
               xTideHeight = GraphLeft;
            }
            else if(x >= GraphLeft + GraphWidth) {
               LOG("Height label moved left to fit\n");
               xTideHeight = GraphWidth - HeighWidth;
            }

            LOG("Draw %s Height @ %d,%d sidth %d\n",
                TideHeight.c_str(),xTideHeight,DrawY,HeighWidth);
            owm->drawString(xTideHeight,DrawY,TideHeight,LEFT,TIDE_PLOT_COLOR);
         }
		}

		SegmentTime = Time - Tides[i].Time;
      SegmentTime /= Tides[i+1].Time - Tides[i].Time; 
		VLOG("t %f Time %d SegmentTime %f i %d %d\n",
          t,Time,SegmentTime,i,Tides[i+i].Time,Tides[i].Time);

		if(SegmentTime < 0.0 || SegmentTime > 1.0) {
			ELOG("Invalid SegmentTime\n");
			return 1;
		}
      ThisPoint = cubicBezier.valueAt(SegmentTime);
		VLOG("%d t %f (%f) = %f\n",DrawX,t,SegmentTime,ThisPoint.y);
      Height = ThisPoint.y;
      DrawY = GraphBottom;
      DrawY -= ((Height - MinHeight) / (MaxHeight - MinHeight)) *
              (GraphBottom - GraphTop);
		VLOG("DrawY %d\n",DrawY);
		if(LastY != 0) {
			owm->drawLine(DrawX,LastY,DrawX + 1,DrawY,TIDE_PLOT_COLOR);
			DrawX++;
		}
		LastY = DrawY;
   }
   return 0;
}

void NoaaTides::InitSegment(
   time_t Start,
   int Segment,
   bezier::Bezier<3> &cubicBezier,
   double &SegmentTime)
{
   std::vector<bezier::Vec2> BezierPoints;
   double MidwayTime;

   MidwayTime = (double) (Tides[Segment+1].Time - Tides[Segment].Time) / 2;
   BezierPoints.push_back({(double)Tides[Segment].Time,
                      (double) Tides[Segment].Height});
   BezierPoints.push_back({MidwayTime,(double) Tides[Segment].Height});
   BezierPoints.push_back({(double) Tides[Segment+1].Time,
                      (double) Tides[Segment+1].Height});
   BezierPoints.push_back({MidwayTime,(double) Tides[Segment+1].Height});
   cubicBezier = bezier::Bezier<3>(BezierPoints);

   SegmentTime = 1.0;
   if(Segment == 0) {
   // previous tide, the entire segment is not plotted
      time_t SegmentDelta = Tides[1].Time - Tides[0].Time;
      SegmentTime = ((double) (Start - Tides[0].Time) / SegmentDelta);
   }

   if(SegmentTime < 0.0 || SegmentTime > 1.0) {
      ELOG("Invalid SegmentTime %f for segment %d\n",SegmentTime,Segment);
   }
}

#endif // WITHOUT_NOAA_TIDES


