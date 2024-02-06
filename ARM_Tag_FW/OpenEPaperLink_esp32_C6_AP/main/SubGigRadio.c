#include "sdkconfig.h"
#ifdef CONFIG_OEPL_SUBGIG_SUPPORT

#include "radio.h"

void SubGig_radio_init(uint8_t ch)
{
}

// uint32_t lastZbTx = 0;
bool SubGig_radioTx(uint8_t *packet) 
{
   return false;
}

void SubGig_radioSetChannel(uint8_t ch)
{
}


int8_t SubGig_commsRxUnencrypted(uint8_t *data)
{
    return 0;
}
#endif
