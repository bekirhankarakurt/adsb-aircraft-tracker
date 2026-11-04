execute_process(COMMAND ${TRACKER} -f iq -
                INPUT_FILE ${INPUT}
                OUTPUT_VARIABLE out
                RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT out MATCHES "THY4KL")
  message(FATAL_ERROR "stdin run failed (rc=${rc}): ${out}")
endif()
