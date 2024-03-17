__main() {
    # The file doesn't have executable permissions, so this shouldn't really happen.
    # Doing this in case someone tries to chmod +x it and execute...

    # shellcheck disable=SC2128,SC2169,SC2039 # ignore array expansion warning
    if [ -n "${BASH_SOURCE-}" ] && [ "${BASH_SOURCE[0]}" = "${0}" ]
    then
        echo "This script should be sourced, not executed:"
        # shellcheck disable=SC2039  # reachable only with bash
        echo ". ${BASH_SOURCE[0]}"
        return 1
    fi

#    if [ $# -ne 1 ]; then
#       echo "com port ? (hint: ttyUSB0)"
#       return 1
#    fi
#
#    export COMPORT=$1
    export COMPORT=ttyACM0
    export ESPPORT=/dev/$COMPORT
    export ADF_PATH=~/esp/esp-adf
    . ~/esp/esp-idf/export.sh
    export PYTHONPATH=$IDF_PATH/tools/ci/python_packages
}

__main $*

