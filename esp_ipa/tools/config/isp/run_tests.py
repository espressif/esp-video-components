# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys
import unittest


def main():
    parser = argparse.ArgumentParser(description='Run ISP configuration unit tests')
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Verbose test output',
    )
    args = parser.parse_args()

    isp_dir = os.path.dirname(os.path.abspath(__file__))
    test_dir = os.path.join(isp_dir, 'test')

    if not os.path.isdir(test_dir):
        print(f'Test directory not found: {test_dir}', file=sys.stderr)
        sys.exit(1)

    sys.path.insert(0, isp_dir)

    loader = unittest.TestLoader()
    suite = loader.discover('test', pattern='test_*.py', top_level_dir=isp_dir)
    runner = unittest.TextTestRunner(verbosity=2 if args.verbose else 1)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
