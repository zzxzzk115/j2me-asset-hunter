# j2me-asset-hunter
Hunt game assets from a J2ME .jar file.

## Pipeline

```mermaid
flowchart LR
    A["J2ME .jar"] --> B["Read ZIP entries"]
    B --> C["Scan binary signatures"]
    C --> D["Recovered PNG images"]
    C --> E["Recovered MIDI"]
    E --> F["FluidSynth · WAV audio"]
```

Recover images and MIDI from J2ME archives, with FluidSynth audio conversion. [Diagram source and validation notes](docs/showcase.md).

## Usage

```bash
./j2me-asset-hunter <your_jar_file.jar> [-o] <output_directory>
```