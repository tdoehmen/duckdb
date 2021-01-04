#include "duckdb/function/aggregate/algebraic_functions.hpp"
#include "duckdb/function/aggregate_function.hpp"

namespace duckdb {

void BuiltinFunctions::RegisterAlgebraicAggregates() {
	Register<AvgFun>();

	Register<CovarSampFun>();
	Register<CovarPopFun>();

	Register<StdDevSampFun>();
	Register<StdDevPopFun>();
	Register<StdDevPopStateFun>();
	Register<VarPopFun>();
	Register<VarSampFun>();
}

} // namespace duckdb
