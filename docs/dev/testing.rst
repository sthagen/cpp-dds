Testing with ``pytest``
#######################

For ``dds``'s more rigorous test suite, we use the ``pytest`` testing framework.
These tests are stored in the ``tests/`` directory and written in ``test_*.py``
files.

The test suite can be run separately without ``dds-ci`` by executing ``pytest``
from within the :doc:`Poetry virtual environment <env>`::

  $ pytest tests/

Note that individual tests can take between a few seconds and a few minutes to
execute, so it may be useful to execute only a subset of the tests based on the
functionality you want to test. Refer to
`the pytest documentation <https://docs.pytest.org/en/latest/>` for more
information about using and executing ``pytest``. If you are running the full
test suite, you may also want to pass the ``-n`` argument with a number of
parallel jobs to execute.


.. highlight:: python

Writing Tests
*************

If a particular aspect of ``dds`` can be tested in isolation and within a few
dozen milliseconds, you should prefer to test it as a unit test in a
``*.test.cpp`` file. The ``pytest`` tests are intended to perform full
end-to-end feature and error handling tests.

Tests are grouped into individual Python files in the ``tests/`` directory. Any
Python file containing tests must have a filename beginning with ``test_``.
Individual test functions should begin with ``test_``. All test functions should
be properly type-annotated and successfully check via ``mypy``.

The ``dds`` test suite has access to a set of test fixtures that can be used
throughout tests to perform complex setup and teardown for complete test-by-test
isolation.

Here is a simple test that simple executes ``dds`` with ``--help``::

  def test_get_help(dds: DDSWrapper) -> None:
      dds.run(['--help'])

In this test function, :func:`the dds object is a test fixture
<dds_ci.testing.fixtures.dds>` that wraps the ``dds`` executable under test.


Testing Error Handling
**********************

It is important that ``dds`` handle errors correctly, of course, including user
error. It is not simply enough to check that a certain operation fails: We must
be sure that it fails *correctly*. To check that the correct code path is
executed, ``dds`` can write a file containing a simple constant string
designating the error handling path that was taken. The file will be written to
the path indicated by the ``DDS_WRITE_ERROR_MARKER`` environment variable.

For examples of these error strings, search for usage of ``write_error_marker``
in the ``dds`` source code. These should only execute within error-handling
contexts, should appear near the log messages that issue diagnostics, and should
be specific to the error at hand.

To write a test that checks for a given error-handling path, use the
:func:`~dds_ci.testing.error.expect_error_marker` context manager function::

  def test_sdist_invalid_project(tmp_project: Project) -> None:
    # Trying to create a package archive from a project without a
    # package.json5 is invalid. Check that it creates the correct
    # error-message string
    with error.expect_error_marker('no-package-json5'):
      tmp_project.pkg_create()

