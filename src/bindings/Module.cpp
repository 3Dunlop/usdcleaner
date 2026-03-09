#include <boost/python.hpp>

namespace bp = boost::python;

// Forward declarations for binding functions defined in other translation units
void wrapPipeline();
void wrapPasses();
void wrapMetrics();

BOOST_PYTHON_MODULE(_usdcleaner) {
    bp::scope().attr("__doc__") =
        "USDCleaner: High-performance USD geometry & material optimization";
    bp::scope().attr("__version__") = "0.1.0";

    wrapPipeline();
    wrapPasses();
    wrapMetrics();
}
