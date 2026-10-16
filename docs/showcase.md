# Pipeline diagram

The diagram describes the current project pipeline. It is documentation, not an application screenshot.

The canonical Mermaid source is [showcase.mmd](showcase.mmd). The README embeds the same definition; [images/showcase.svg](images/showcase.svg) is an SVG export for reuse on the personal website. Rectangular nodes and a plain background avoid added rounded frames.

Validated on 2026-10-16 in a new independent workspace at source checkout `41b10d132a9af80174c72b7091c00298d73d6c43`.

The source uses libzip for archive reading, scans PNG/MIDI signatures, and uses FluidSynth for MIDI audio conversion. A self-made JAR fixture recovered three PNGs in a fresh output directory; this test did not exercise the MIDI path. The fixture and extracted PNGs are in `showcase/`.
