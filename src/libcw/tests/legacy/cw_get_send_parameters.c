#include "libcw_gen.h"
#include "cw_get_send_parameters.h"
#include "common.h"




/**
   @reviewed on 2019-10-13
*/
int legacy_api_test_cw_get_send_parameters(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	bool success = true;;
	for (int speed = CW_SPEED_MIN; speed <= CW_SPEED_MAX; speed++) {

		cw_set_send_speed(speed);

		cw_gen_duration_parameters_t params = {
			.dot_duration = -1,
			.dash_duration = -1,
			.ims_duration = -1,
			.ics_duration = -1,
			.iws_duration = -1,
			.additional_space_duration = -1,
			.adjustment_space_duration = -1,
		};

		LIBCW_TEST_FUT(cw_get_send_parameters)
			(&params.dot_duration,
			 &params.dash_duration,
			 &params.ims_duration,
			 &params.ics_duration,
			 &params.iws_duration,
			 &params.additional_space_duration,
			 &params.adjustment_space_duration);

		cte->cte_log(cte, LOG_DEBUG,
		             "generator's sending parameters @ %02d wpm:\n"
		             "    dot duration = %07d us\n"
		             "   dash duration = %07d us\n"
		             "    ims duration = %07d us\n"
		             "    ics duration = %07d us\n"
		             "    iws duration = %07d us\n"
		             "additional space duration = %07d us\n"
		             "adjustment space duration = %07d us\n",
		             speed,
		             params.dot_duration,
		             params.dash_duration,
		             params.ims_duration,
		             params.ics_duration,
		             params.iws_duration,
		             params.additional_space_duration,
		             params.adjustment_space_duration);

		success = success && (cwt_retv_ok == test_gen_params_relations(cte, &params, speed));
	}

	const bool final_success = cte->expect_op_int(cte, success, "==", true, "Getting generator parameters");

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	if (final_success) {
		return cwt_retv_ok;
	} else {
		return cwt_retv_err;
	}
}




