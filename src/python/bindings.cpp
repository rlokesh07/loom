// src/python/bindings.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "../../include/Tool.hpp"
#include "../../include/Parameter.hpp"
#include "../../include/ToolResult.hpp"

#include "../../examples/CalculatorTool.hpp"

namespace py = pybind11;

PYBIND11_MODULE(calculator_tools, m) {
    
}
