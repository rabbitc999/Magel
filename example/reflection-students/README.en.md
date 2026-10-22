# Reflection-based student manager

A command-line student information manager. The model is declared with `@Reflect`
and SQL model annotations, the field list is read through reflection, JSON encoding
and decoding use the reflection serializer, and every database access is
parameterized SQLite.

Verified commands: `schema`, `add`, `update`, `get`, `delete`, `list` (with optional
field sorting) and `search`, plus validation of id, name, class, age and score, and
the error paths for malformed JSON, unknown sort fields, bad sort directions and
missing ids.

Run the source with `chali main.chl DATABASE COMMAND [ARGS]`. Commands: `add JSON`,
`update JSON`, `get ID`, `delete ID`, `list [FIELD [asc|desc]]`, `search NAME`,
`schema`. Record fields: `id`, `name`, `age`, `className`, `score`.
