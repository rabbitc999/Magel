# Reflection-based student manager draft

This is an unaccepted handoff draft. It uses reflection metadata, SQL model annotations, reflection JSON codecs and parameterized SQLite. Empty listing, insertion, lookup and persistent listing were exercised through the shared VM. Update, delete, search, sorting, invalid input and full cleanup validation remain pending.

Run source with `magel main.mgl DATABASE COMMAND [ARGS]`. Commands: `add JSON`, `update JSON`, `get ID`, `delete ID`, `list [FIELD [asc|desc]]`, `search NAME`, `schema`. Record fields: `id`, `name`, `age`, `className`, `score`. See [TASK.md](../../TASK.md) for the remaining work.
