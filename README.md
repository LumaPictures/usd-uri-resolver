# USD-URI-resolver
A generic, URI based resolver for USD, support custom plugins.

## Project Goals
* Resolve custom URI based for USD files.
* Provide a generic plugin registry to extend the resolver without modifying its source code.
* Include example resolvers supporting database based files.

## Features

### Current
* URIResolver - custom resolver for USD.
* usd_sql::SQL - MySQL database access.
* obfuscate_pass - A simple tool to convert passwords to a z85 encoded string. WARNING !!! This is not for encrypting your password, but to hide it from artists in an environment. It is extremely simple to "decrypt" and offers no protection.

### Planned
* Create a plugin registry for extending URIResolver.
* Move the SQL accessor to a spearete plugin and load it dynamically.
* Windows support.
* Simplify the build process.

## Building

You'll need the following libraries to build the project; newer versions most likely work as well, but they are not tested. Currently, the cmake files are looking for the openexr libs, but only use the half headers. This will be removed in the future.

| Package           | Version        |
| ----------------- | -------------- |
| USD               | 0.7.6+ (stock) |
| TBB               | 4.3+           |
| OpenEXR           | 2.2.0+         |
| Boost             | 1.61.0+        |
| MySQL Connector C | 6.1.9+         |
| CMAKE             | 2.8+           |

You can download the MySql library [here](https://dev.mysql.com/downloads/connector/c/).

There are two main ways to configure a library location. 1, configure an environment variable. 2, pass the location of the library using -D\<varname\>=\<path\> to cmake. This will be simplified soon, once we add proper find modules.

* Point the USD\_ROOT environment variable to the location of the installed USD.
* Point the MYSQL\_CONNECTOR\_ROOT variable to the location of the extracted MySql connector C library.
* Pass OPENEXR\_LOCATION to the CMake command or setup the OPENEXR\_LOCATION environment variable. They have to point at a standard build of OpenEXR, including IlmBase.
* Point TBB\_ROOT\_DIR}, TBB\_INSTALL\_DIR or TBBROOT at your local TBB installation.

## Contributing
TODO.

## Using the SQL resolver.
Consult the README.md installed alongside the URIResolver.