# Usage of the Uber Resolver

Currently the Uber Resolver provides support for an alternative protocol, when referencing other usd files, namely loading data from an SQL database.

## SQL Protocol

#### Request URL

SQL procotol can be accessed, by using sql://<server_name><asset_path> when referencing assets in a USD file.

Server name
- Can be either an IP address or a host name.

Asset path
- Need to start with /, just like on a normal file SYSTEM
- Need to end with an extension, that represent the data storage format in the database (usd currently)

#### Table layout

The SQL resolver expects a table, with 3 entries.
- path - CHAR / VARCHAR containing the path to the asset
- data - (LONG/MEDIUM/SHORT)BLOB containing the data.
- time - TIMESTAMP containing the last asset modification time. Set the expression to ON UPDATE CURRENT_TIMESTAMP to always keep up to date with changes, and make sure timezones are setup correctly on the databases.

#### Environment variables supported by the resolver

Each environment variable can be either setup globally, or server specific. First the server specific variable is queried, then the global one, then the default value is used. Server specific variables can be setup by prefixing the environment variable with <server_name>_ . For example USD_SQL_PASSWD becomes sv-dev01.luma.mel_USD_SQL_PASSWD if specialized for that given server.

- USD_SQL_DB - Database name on the SQL server. Default value is usd.
- USD_SQL_USER - User to access the database. Default value is root.
- USD_SQL_PASSWD - Password for the user to access the database. Default value is the obfuscated version of 12345678.
- USD_SQL_PORT - Port to access the database. Default value is 3306.
- USD_SQL_TABLE - Name of the table containing the data. Default value is headers.
- USD_SQL_CACHE_PATH - Name of the local cache path to save usd files. Default value is /tmp.

#### Password obfuscation

To avoid storing passwords directly in pipeline files (typically python), the resolver provides a small application that obfuscates passwords. The usage is simple, just call uber_resolver_obfuscate_pass <password> and use the returned value when setting up environment variables. The goal of this is not to provide absolute safety, but to hide passwords from the non-coder eyes.
