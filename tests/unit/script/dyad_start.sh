flux kvs namespace create ${DYAD_KVS_NAMESPACE}
flux exec -r all flux module load ${DYAD_MODULE_SO} --info_log=${DYAD_LOG_DIR}/dyad-broker --error_log=${DYAD_LOG_DIR}/dyad-broker --mode=${DYAD_DTL_MODE} $DYAD_PATH