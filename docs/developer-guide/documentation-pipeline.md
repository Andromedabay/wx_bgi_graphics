# Documentation Pipeline

This project keeps its narrative documentation in markdown and feeds that
content into Doxygen to generate HTML and PDF output.

## Source of Truth

- The main landing page is **[README.md](../../README.md)**
- Guide landing pages live under:
  - `docs/user-guide/`
  - `docs/developer-guide/`
- Detailed reference/content pages still live at the repository root

## Doxygen Configuration

The documentation build is driven by:

- `docs/Doxyfile.in`

That file controls:

- which markdown pages are included
- which headers are scanned
- which file acts as the main page
- the image search path
- HTML and LaTeX/PDF generation

## Build Targets

### HTML

```powershell
cmake --build build --target api_docs -j --config Debug
```

### HTML + PDF

```powershell
cmake --build build --target api_docs_pdf -j --config Debug
```

The PDF build requires a working LaTeX toolchain.

## Images

Markdown image references are resolved through the Doxygen `IMAGE_PATH`
configuration so screenshots from `images/` appear in generated HTML and PDF.

## Maintenance Guidance

When restructuring docs:

1. Keep `README.md` concise and high-level
2. Update the guide landing pages if pages are added, moved, or retitled
3. Keep `docs/Doxyfile.in` aligned with the real markdown layout
4. Rebuild both HTML and PDF outputs after documentation changes

## Related Pages

- **[Building.md](./Building.md)** for current doc build commands
- **[Developer Guide index](./README.md)** for the rest of the internal docs
