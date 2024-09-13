## CLion setup (Linux)

Prerequisites:

- CMake
- Ninja

Ubuntu installation:

```
sudo apt install cmake ninja-build
```

Fedora installation:

```
sudo dnf install cmake ninja-build
```

### C++ tests

Go to "Run/Debug configurations" -> "Edit configuration templates" (bottom left) -> "Google Test". 

Add `"ClickHouse DSN (ANSI)"` to the default profile program arguments. Now, you should be able to run the integration tests from the CLion IDE.


## Python tests

E2E tests are using [pyodbc](https://pypi.org/project/pyodbc/) and [pytest](https://docs.pytest.org/en/latest/index.html).

### PyCharm setup

Verified with PyCharm 2024.1 and Python 3.12.

* Open the `test` directory in PyCharm
* Create a new `venv` interpreter using PyCharm
* Install all dependencies from the `requirements.txt` file

Now, you should be able to run the tests from the PyCharm itself.

### Running the tests from the CLI

Having a [virtual environment](https://docs.python.org/3/library/venv.html) set up for the project (recommended), you can run the tests from the console:

```
cd tests
pip install -r requirements.txt
docker-compose up -d
pytest
```
