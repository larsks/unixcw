#ifndef _LIBCW_TQ_TESTS_H_
#define _LIBCW_TQ_TESTS_H_




#include "libcw_test.h"
#include "libcw_gen.h"




unsigned int test_cw_tq_init_internal(void);
unsigned int test_cw_tq_enqueue_internal_2(void);
unsigned int test_cw_tq_test_capacity_1(cw_test_stats_t * stats);
unsigned int test_cw_tq_test_capacity_2(cw_test_stats_t * stats);
unsigned int test_cw_tq_wait_for_level_internal(cw_test_stats_t * stats);
unsigned int test_cw_tq_is_full_internal(cw_test_stats_t * stats);
unsigned int test_cw_tq_enqueue_dequeue_internal(cw_test_stats_t * stats);
unsigned int test_cw_tq_enqueue_args_internal(cw_gen_t * gen, cw_test_stats_t * stats);
unsigned int test_cw_tq_new_delete_internal(cw_test_stats_t * stats);
unsigned int test_cw_tq_get_capacity_internal(cw_test_stats_t * stats);
unsigned int test_cw_tq_length_internal(cw_test_stats_t * stats);
unsigned int test_cw_tq_callback(cw_gen_t * gen, cw_test_stats_t * stats);
unsigned int test_cw_tq_prev_index_internal(cw_test_stats_t * stats);
unsigned int test_cw_tq_next_index_internal(cw_test_stats_t * stats);
unsigned int test_cw_tq_operations_1(cw_gen_t * gen, cw_test_stats_t * stats);
unsigned int test_cw_tq_operations_2(cw_gen_t * gen, cw_test_stats_t * stats);
unsigned int test_cw_tq_operations_3(cw_gen_t * gen, cw_test_stats_t * stats);




#endif /* #ifndef _LIBCW_TQ_TESTS_H_ */
