/*
 * Copyright 2018 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Gabe Black
 */

#ifndef __SYSTEMC_EXT_CORE__USING_HH__
#define __SYSTEMC_EXT_CORE__USING_HH__

#include "_core.hh"

using sc_core::sc_attr_base;
using sc_core::sc_attribute;
using sc_core::sc_attr_cltn;

using sc_core::sc_event_finder;
using sc_core::sc_event_finder_t;
using sc_core::sc_event_and_list;
using sc_core::sc_event_or_list;
using sc_core::sc_event_and_expr;
using sc_core::sc_event_or_expr;
using sc_core::sc_event;
using sc_core::sc_get_top_level_events;
using sc_core::sc_find_event;

using sc_core::sc_export_base;
using sc_core::sc_export;

using sc_core::sc_interface;

using sc_core::sc_argc;
using sc_core::sc_argv;
using sc_core::sc_starvation_policy;
using sc_core::SC_RUN_TO_TIME;
using sc_core::SC_EXIT_ON_STARVATION;
using sc_core::sc_start;
using sc_core::sc_pause;
using sc_core::sc_set_stop_mode;
using sc_core::sc_get_stop_mode;
using sc_core::sc_stop_mode;
using sc_core::SC_STOP_FINISH_DELTA;
using sc_core::SC_STOP_IMMEDIATE;
using sc_core::sc_stop;
using sc_core::sc_time_stamp;
using sc_core::sc_delta_count;
using sc_core::sc_is_running;
using sc_core::sc_pending_activity_at_current_time;
using sc_core::sc_pending_activity_at_future_time;
using sc_core::sc_pending_activity;
using sc_core::sc_time_to_pending_activity;
using sc_core::sc_get_status;
using sc_core::SC_ELABORATION;
using sc_core::SC_BEFORE_END_OF_ELABORATION;
using sc_core::SC_END_OF_ELABORATION;
using sc_core::SC_START_OF_SIMULATION;
using sc_core::SC_RUNNING;
using sc_core::SC_PAUSED;
using sc_core::SC_STOPPED;
using sc_core::SC_END_OF_SIMULATION;
using sc_core::SC_END_OF_INITIALIZATION;
using sc_core::SC_END_OF_UPDATE;
using sc_core::SC_BEFORE_TIMESTEP;
using sc_core::SC_STATUS_ANY;
using sc_core::sc_status;

using sc_core::sc_bind_proxy;
using sc_core::SC_BIND_PROXY_NIL;
using sc_core::sc_module;
using sc_core::next_trigger;
using sc_core::wait;
using sc_core::halt;
using sc_core::sc_gen_unique_name;
using sc_core::sc_behavior;
using sc_core::sc_channel;
using sc_core::sc_start_of_simulation_invoked;
using sc_core::sc_end_of_simulation_invoked;

using sc_core::sc_module_name;

using sc_core::sc_object;
using sc_core::sc_get_top_level_objects;
using sc_core::sc_find_object;

using sc_core::sc_port_policy;
using sc_core::SC_ONE_OR_MORE_BOUND;
using sc_core::SC_ZERO_OR_MORE_BOUND;
using sc_core::SC_ALL_BOUND;
using sc_core::sc_port_base;
using sc_core::sc_port_b;
using sc_core::sc_port;

using sc_core::sc_prim_channel;

using sc_core::sc_curr_proc_kind;
using sc_core::SC_NO_PROC_;
using sc_core::SC_METHOD_PROC_;
using sc_core::SC_THREAD_PROC_;
using sc_core::SC_CTHREAD_PROC_;
using sc_core::sc_descendent_inclusion_info;
using sc_core::SC_NO_DESCENDANTS;
using sc_core::SC_INCLUDE_DESCENDANTS;
using sc_core::sc_unwind_exception;
using sc_core::sc_process_handle;
using sc_core::sc_get_current_process_handle;
using sc_core::sc_is_unwinding;

using sc_core::sc_sensitive;

using sc_core::sc_spawn_options;
using sc_core::sc_spawn;

using sc_core::sc_time_unit;
using sc_core::SC_FS;
using sc_core::SC_PS;
using sc_core::SC_NS;
using sc_core::SC_US;
using sc_core::SC_MS;
using sc_core::SC_SEC;
using sc_core::sc_time;
using sc_core::sc_time_tuple;
using sc_core::SC_ZERO_TIME;
using sc_core::sc_set_time_resolution;
using sc_core::sc_get_time_resolution;
using sc_core::sc_max_time;
using sc_core::sc_get_default_time_unit;
using sc_core::sc_set_default_time_unit;

#endif  //__SYSTEMC_EXT_CORE__USING_HH__
