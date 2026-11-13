# Security policy

## Reporting a vulnerability

Please do not open a public issue for a security problem. Use GitHub's private reporting instead: open the [Security tab](../../security), choose "Report a vulnerability" and describe the problem, with steps to reproduce if you can. The report stays private until a fix is out.

Reports are answered as soon as possible, usually within a few days. This is a hobby project, so there is no fixed deadline.

## Supported versions

Only the latest release gets fixes. Please check that the problem is still there in the latest release before reporting.

## Scope

In scope:

- parsing of data from the TMX API (map lists, map files) that could crash the game or run code
- files the plugin writes and where it writes them
- a way to make the plugin run while the game is online

Out of scope:

- vulnerabilities in the Horizon Client, the game or TMX; report those to their own projects (the Horizon Client has its own [security policy](https://github.com/cheatoskar/horizon-client/security/policy))
- problems that need an attacker who already controls your PC
