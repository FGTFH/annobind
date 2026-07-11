import os
import sys

sys.path.append(os.getcwd())

import bin.ab_test.inner as inner
from bin import ab_test

ab_test.f_int(2)
ab_test.MyClass.greet(ab_test.MyClass())
arr = ab_test.IntArray10()
arr.fill(1)
print(arr.at(0))
print(ab_test.MyClass.staticConstexprInt)
