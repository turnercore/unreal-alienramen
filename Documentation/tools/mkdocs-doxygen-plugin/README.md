# Vendored MkDocs Doxygen Plugin

This is a minimal vendored fork of `mkdocs-doxygen-plugin`.

It exists because the upstream package currently imports `Sequence` from
`collections`, which breaks on modern Python versions used in this repo's
docs environment. The functional change in this vendored copy is the Python
compatibility import in `configitems.py`.

Upstream project:
- <https://github.com/pieterdavid/mkdocs-doxygen-plugin>
