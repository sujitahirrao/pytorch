"""
To run this file by hand from the root of the PyTorch
repository, run:

python -m tools.autograd.gen_autograd \
       build/aten/src/ATen/Declarations.yaml \
       aten/src/ATen/native/native_functions.yaml \
       $OUTPUT_DIR \
       tools/autograd

Where $OUTPUT_DIR is where you would like the files to be
generated.  In the full build system, OUTPUT_DIR is
torch/csrc/autograd/generated/
"""

# gen_autograd.py generates C++ autograd functions and Python bindings.
#
# It delegates to the following scripts:
#
#  gen_autograd_functions.py: generates subclasses of torch::autograd::Node
#  gen_variable_type.py: generates VariableType.h which contains all tensor methods
#  gen_python_functions.py: generates Python bindings to THPVariable
#

import argparse
import os
from tools.codegen.api import cpp
from tools.codegen.api.autograd import (
    match_differentiability_info, NativeFunctionWithDifferentiabilityInfo,
)
from tools.codegen.gen import parse_native_yaml
from tools.codegen.selective_build.selector import SelectiveBuilder
from typing import List

def gen_autograd(
    aten_path: str,
    native_functions_path: str,
    out: str,
    autograd_dir: str,
    operator_selector: SelectiveBuilder,
    disable_autograd: bool = False,
) -> None:
    # Parse and load derivatives.yaml
    from .load_derivatives import load_derivatives
    differentiability_infos = load_derivatives(
        os.path.join(autograd_dir, 'derivatives.yaml'), native_functions_path)

    template_path = os.path.join(autograd_dir, 'templates')

    fns = list(sorted(filter(
        operator_selector.is_native_function_selected_for_training,
        parse_native_yaml(native_functions_path)), key=lambda f: cpp.name(f.func)))
    fns_with_diff_infos: List[NativeFunctionWithDifferentiabilityInfo] = match_differentiability_info(fns, differentiability_infos)

    # Generate VariableType.h/cpp
    from .gen_trace_type import gen_trace_type
    from .gen_variable_type import gen_variable_type
    if not disable_autograd:
        gen_variable_type(out, native_functions_path, fns_with_diff_infos, template_path)

        # operator filter not applied as tracing sources are excluded in selective build
        gen_trace_type(out, native_functions_path, template_path)

    # Generate Functions.h/cpp
    from .gen_autograd_functions import gen_autograd_functions_lib
    gen_autograd_functions_lib(
        out, differentiability_infos, template_path)

    # Generate variable_factories.h
    from .gen_variable_factories import gen_variable_factories
    gen_variable_factories(out, native_functions_path, template_path)


def gen_autograd_python(
    aten_path: str,
    native_functions_path: str,
    out: str,
    autograd_dir: str,
) -> None:
    from .load_derivatives import load_derivatives
    differentiability_infos = load_derivatives(
        os.path.join(autograd_dir, 'derivatives.yaml'), native_functions_path)

    template_path = os.path.join(autograd_dir, 'templates')

    # Generate Functions.h/cpp
    from .gen_autograd_functions import gen_autograd_functions_python
    gen_autograd_functions_python(
        out, differentiability_infos, template_path)

    # Generate Python bindings
    from . import gen_python_functions
    deprecated_path = os.path.join(autograd_dir, 'deprecated.yaml')
    gen_python_functions.gen(
        out, native_functions_path, deprecated_path, template_path)


def main() -> None:
    parser = argparse.ArgumentParser(
        description='Generate autograd C++ files script')
    parser.add_argument('declarations', metavar='DECL',
                        help='path to Declarations.yaml')
    parser.add_argument('native_functions', metavar='NATIVE',
                        help='path to native_functions.yaml')
    parser.add_argument('out', metavar='OUT',
                        help='path to output directory')
    parser.add_argument('autograd', metavar='AUTOGRAD',
                        help='path to autograd directory')
    args = parser.parse_args()
    gen_autograd(args.declarations, args.native_functions,
                 args.out, args.autograd,
                 SelectiveBuilder.get_nop_selector())


if __name__ == '__main__':
    main()
