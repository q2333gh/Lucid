jwk@jwk-vm ~/c/Lucid (master)> python examples/records/records_test.py
============================================================
Running records test suite
============================================================
====================================================================================== test session starts ======================================================================================
platform linux -- Python 3.10.12, pytest-9.0.2, pluggy-1.6.0 -- /home/jwk/my_uv_venv/daily_ic_dev/.venv/bin/python3
cachedir: .pytest_cache
rootdir: /home/jwk/code/Lucid
plugins: anyio-4.12.0
collected 9 items                                                                                                                                                                               

examples/records/records_test.py::test_greet PASSED                                                                                                                                       [ 11%]
examples/records/records_test.py::test_add_user PASSED                                                                                                                                    [ 22%]
examples/records/records_test.py::test_get_address PASSED                                                                                                                                 [ 33%]
examples/records/records_test.py::test_get_profile PASSED                                                                                                                                 [ 44%]
examples/records/records_test.py::test_get_user_info PASSED                                                                                                                               [ 55%]
examples/records/records_test.py::test_get_nested_data PASSED                                                                                                                             [ 66%]
examples/records/records_test.py::test_get_optional_data_with_age PASSED                                                                                                                  [ 77%]
examples/records/records_test.py::test_get_optional_data_without_age PASSED                                                                                                               [ 88%]
examples/records/records_test.py::test_get_complex_record PASSED                                                                                                                          [100%]

====================================================================================== 9 passed in 25.39s =======================================================================================