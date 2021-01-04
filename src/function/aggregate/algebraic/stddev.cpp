#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/function/aggregate/algebraic_functions.hpp"
#include "duckdb/function/function_set.hpp"

#include <cmath>
#include <utility>

namespace duckdb {

struct stddev_state_t {
	uint64_t count;  //  n
	double mean;     //  M1
	double dsquared; //  M2
};

// Streaming approximate standard deviation using Welford's
// method, DOI: 10.2307/1266577
struct STDDevBaseOperation {
	template <class STATE> static void Initialize(STATE *state) {
		state->count = 0;
		state->mean = 0;
		state->dsquared = 0;
	}

	template <class INPUT_TYPE, class STATE, class OP>
	static void Operation(STATE *state, INPUT_TYPE *input_data, nullmask_t &nullmask, idx_t idx) {
		// update running mean and d^2
		state->count++;
		const double input = input_data[idx];
		const double mean_differential = (input - state->mean) / state->count;
		const double new_mean = state->mean + mean_differential;
		const double dsquared_increment = (input - new_mean) * (input - state->mean);
		const double new_dsquared = state->dsquared + dsquared_increment;

		state->mean = new_mean;
		state->dsquared = new_dsquared;
	}

	template <class INPUT_TYPE, class STATE, class OP>
	static void ConstantOperation(STATE *state, INPUT_TYPE *input_data, nullmask_t &nullmask, idx_t count) {
		for (idx_t i = 0; i < count; i++) {
			Operation<INPUT_TYPE, STATE, OP>(state, input_data, nullmask, 0);
		}
	}

	template <class STATE, class OP> static void Combine(STATE source, STATE *target) {
		if (target->count == 0) {
			*target = source;
		} else if (source.count > 0) {
			const auto count = target->count + source.count;
			const auto mean = (source.count * source.mean + target->count * target->mean) / count;
			const auto delta = source.mean - target->mean;
			target->dsquared =
			    source.dsquared + target->dsquared + delta * delta * source.count * target->count / count;
			target->mean = mean;
			target->count = count;
		}
	}

	static bool IgnoreNull() {
		return true;
	}
};

struct VarSampOperation : public STDDevBaseOperation {
	template <class T, class STATE>
	static void Finalize(Vector &result, FunctionData *, STATE *state, T *target, nullmask_t &nullmask, idx_t idx) {
		if (state->count == 0) {
			nullmask[idx] = true;
		} else {
			target[idx] = state->count > 1 ? (state->dsquared / (state->count - 1)) : 0;
			if (!Value::DoubleIsValid(target[idx])) {
				throw OutOfRangeException("VARSAMP is out of range!");
			}
		}
	}
};

struct STDDevSampStateOperation : public STDDevBaseOperation {
	template <class T, class STATE>
	static void Finalize(Vector &result, FunctionData *, STATE *state, T *target, nullmask_t &nullmask, idx_t idx) {
		if (state->count == 0) {
			nullmask[idx] = true;
		} else {
			double stddev_samp = state->count > 1 ? sqrt(state->dsquared / (state->count - 1)) : 0;

			if (!Value::DoubleIsValid(stddev_samp)) {
				throw OutOfRangeException("STDDEV_SAMP is out of range!");
			}

			std::string states = "{";
			states = states + "count: " + std::to_string(state->count) + ", ";
			states = states + "mean: " + std::to_string(state->mean) + ", ";
			states = states + "dsquared: " + std::to_string(state->dsquared) + ", ";
			states = states + "stddev: " + std::to_string(stddev_samp) + "}";
			target[idx] = StringVector::AddString(result, states.c_str(), states.size());
		}
	}
};

struct VarPopOperation : public STDDevBaseOperation {
	template <class T, class STATE>
	static void Finalize(Vector &result, FunctionData *, STATE *state, T *target, nullmask_t &nullmask, idx_t idx) {
		if (state->count == 0) {
			nullmask[idx] = true;
		} else {
			target[idx] = state->count > 1 ? (state->dsquared / state->count) : 0;
			if (!Value::DoubleIsValid(target[idx])) {
				throw OutOfRangeException("VARPOP is out of range!");
			}
		}
	}
};

struct STDDevSampOperation : public STDDevBaseOperation {
	template <class T, class STATE>
	static void Finalize(Vector &result, FunctionData *, STATE *state, T *target, nullmask_t &nullmask, idx_t idx) {
		if (state->count == 0) {
			nullmask[idx] = true;
		} else {
			target[idx] = state->count > 1 ? sqrt(state->dsquared / (state->count - 1)) : 0;
			if (!Value::DoubleIsValid(target[idx])) {
				throw OutOfRangeException("STDDEV_SAMP is out of range!");
			}
		}
	}
};

struct STDDevPopOperation : public STDDevBaseOperation {
	template <class T, class STATE>
	static void Finalize(Vector &result, FunctionData *, STATE *state, T *target, nullmask_t &nullmask, idx_t idx) {
		if (state->count == 0) {
			nullmask[idx] = true;
		} else {
			target[idx] = state->count > 1 ? sqrt(state->dsquared / state->count) : 0;
			if (!Value::DoubleIsValid(target[idx])) {
				throw OutOfRangeException("STDDEV_POP is out of range!");
			}
		}
	}
};


void StdDevSampFun::RegisterFunction(BuiltinFunctions &set) {
	AggregateFunctionSet stddev_samp("stddev_samp");
	stddev_samp.AddFunction(AggregateFunction::UnaryAggregate<stddev_state_t, double, double, STDDevSampOperation>(
	    LogicalType::DOUBLE, LogicalType::DOUBLE));
	set.AddFunction(stddev_samp);
	AggregateFunctionSet stddev("stddev");
	stddev.AddFunction(AggregateFunction::UnaryAggregate<stddev_state_t, double, double, STDDevSampOperation>(
	    LogicalType::DOUBLE, LogicalType::DOUBLE));
	set.AddFunction(stddev);
}

void StdDevPopFun::RegisterFunction(BuiltinFunctions &set) {
	AggregateFunctionSet stddev_pop("stddev_pop");
	stddev_pop.AddFunction(AggregateFunction::UnaryAggregate<stddev_state_t, double, double, STDDevPopOperation>(
	    LogicalType::DOUBLE, LogicalType::DOUBLE));
	set.AddFunction(stddev_pop);
}

void StdDevPopStateFun::RegisterFunction(BuiltinFunctions &set) {
	AggregateFunctionSet stddev_state("stddev_state");
	stddev_state.AddFunction(
	    AggregateFunction::UnaryAggregate<stddev_state_t, double, string_t, STDDevSampStateOperation>(
	        LogicalType::DOUBLE, LogicalType::VARCHAR));
	set.AddFunction(stddev_state);
}

void VarPopFun::RegisterFunction(BuiltinFunctions &set) {
	AggregateFunctionSet var_pop("var_pop");
	var_pop.AddFunction(AggregateFunction::UnaryAggregate<stddev_state_t, double, double, VarPopOperation>(
	    LogicalType::DOUBLE, LogicalType::DOUBLE));
	set.AddFunction(var_pop);
}

void VarSampFun::RegisterFunction(BuiltinFunctions &set) {
	AggregateFunctionSet var_samp("var_samp");
	var_samp.AddFunction(AggregateFunction::UnaryAggregate<stddev_state_t, double, double, VarSampOperation>(
	    LogicalType::DOUBLE, LogicalType::DOUBLE));
	set.AddFunction(var_samp);
}

} // namespace duckdb
