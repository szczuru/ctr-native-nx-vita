# Switch NRO assets

Place the following file here before building:

```
switch/
  icon.jpg    — 256×256 JPEG, shown in the Homebrew Menu
  ctr_native.nacp  — generated automatically by nacptool during build
```

## Minimum viable icon

```bash
# Requires ImageMagick
convert -size 256x256 xc:#1a1a2e -fill white \
    -gravity Center -pointsize 20 -annotate 0 "Crash Team Racing\nNative" \
    switch/icon.jpg
```

Or use any 256×256 JPEG.

## NACP

The `.nacp` file is generated automatically by the CMake build via `nacptool`.
You do not need to create it manually.
