# Validators

There are two forms of validators:

- `transform` validators: mutating
- `check` validators: non-mutating (recommended unless the parsed string must be
  mutated)

A transform validator comes in one form, a function with the signature
`std::string(std::string&)`. The function will take a string and return an error
message, or an empty string if input is valid. If there is an error, the
function should throw a `CLI::ValidationError` with the appropriate reason as a
message.

An example of a mutating validator:

```cpp
auto transform_validator = CLI::Validator(
        [](std::string &input) {
            if (input == "error") {
                return "error is not a valid value";
            } else if (input == "unexpected") {
                throw CLI::ValidationError{"Unexpected error"};
            }
            input = "new string";
            return "";
        }, "VALIDATOR DESCRIPTION", "Validator name");

cli_global.add_option("option")->transform(transform_validator);
```

However, `check` validators come in two forms; either a simple function with the
const version of the above signature, `std::string(const std::string &)`, or a
subclass of `struct CLI::Validator`. This structure has two members that a user
should set; one (`func_`) is the function to add to the Option (exactly matching
the above function signature, since it will become that function), and the other
is `name_`, and is the type name to set on the Option (unless empty, in which
case the typename will be left unchanged).

Validators can be combined with `&` and `|`, and they have an `operator()` so
that you can call them as if they were a function. In CLI11, const static
versions of the validators are provided so that the user does not have to call a
constructor also.

An example of a custom validator:

```cpp
struct LowerCaseValidator : public Validator {
    LowerCaseValidator() {
        name_ = "LOWER";
        func_ = [](const std::string &str) {
            if(CLI::detail::to_lower(str) != str)
                return std::string("String is not lower case");
            else
                return std::string();
        };
    }
};
const static LowerCaseValidator Lowercase;
```

If you were not interested in the extra features of Validator, you could simply
pass the lambda function above to the `->check()` method of `Option`.

The built-in validators for CLI11 are:

| Validator           | Description                                                            |
| ------------------- | ---------------------------------------------------------------------- |
| `ExistingFile`      | Check for existing file (returns error message if check fails)         |
| `ExistingDirectory` | Check for an existing directory (returns error message if check fails) |
| `ExistingPath`      | Check for an existing path                                             |
| `NonexistentPath`   | Check for an non-existing path                                         |
| `Range(min=0, max)` | Produce a range (factory). Min and max are inclusive.                  |

And, the protected members that you can set when you make your own are:

| Type                                        | Member               | Description                                                            |
| ------------------------------------------- | -------------------- | ---------------------------------------------------------------------- |
| `std::function<std::string(std::string &)>` | `func_`              | Core validation function - modifies input and returns "" if successful |
| `std::function<std::string()>`              | `desc_function`      | Optional description function (uses `description_` instead if not set) |
| `std::string`                               | `name_`              | The name for search purposes                                           |
| `int` (`-1`)                                | `application_index_` | The element this validator applies to (-1 for all)                     |
| `bool` (`true`)                             | `active_`            | This can be disabled                                                   |
| `bool` (`false`)                            | `non_modifying_`     | Specify that this is a Validator instead of a Transformer              |
