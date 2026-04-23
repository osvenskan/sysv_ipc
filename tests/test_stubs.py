import ast
import pathlib
import sys
import unittest

import sysv_ipc
from .base import Base

_STUB_PATH = pathlib.Path(__file__).parent.parent / 'src' / '__init__.pyi'


def _names_defined_in_stub():
    '''Return the set of top-level non-dunder names declared in the .pyi stub.'''
    tree = ast.parse(_STUB_PATH.read_text())
    defined = set()
    for node in ast.iter_child_nodes(tree):
        if isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            defined.add(node.target.id)
        elif isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    defined.add(target.id)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            defined.add(node.name)
        elif isinstance(node, ast.If):
            # Platform guards (e.g. `if sys.platform == "linux":`)
            # If we eve add more conditionals to the pyi file we'll need to
            # extend the logic here (but until then, YAGNI)
            if sys.platform != 'linux':
                continue
            for child in ast.walk(node):
                if isinstance(child, ast.AnnAssign) and isinstance(child.target, ast.Name):
                    defined.add(child.target.id)
    return defined


class TestStubs(Base):
    '''Verify that the .pyi stub covers every name exported by __all__.'''

    def test_all_entries_present_in_stub(self):
        '''Every name in __all__ must have a declaration in the stub.'''
        defined = _names_defined_in_stub()
        missing = [name for name in sysv_ipc.__all__ if name not in defined]
        self.assertFalse(
            missing,
            f"Names in __all__ missing from stub: {missing}",
        )

    def test_no_extra_names_in_stub(self):
        '''Every non-dunder name in the stub must appear in __all__.'''
        defined = _names_defined_in_stub()
        extra = [
            name for name in defined
            if not name.startswith('__') and name not in sysv_ipc.__all__
        ]
        self.assertFalse(
            extra,
            f"Names in stub not covered by __all__: {extra}",
        )


if __name__ == '__main__':
    unittest.main()
