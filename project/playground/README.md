# Switchboard Lab

An interactive page for playing with the ideas in this project: break switches on the satellite (or four other small systems) and watch the counting vector, tail signature, signature and MTTF change; step through the level sweep; flip switches in random order; compare failure models; and compare repair policies, including a step-by-step Dinkelbach run.

Open `switchboard-lab.html` in a browser. It is one self-contained file (it only loads fonts from Google Fonts, and falls back to system fonts offline).

| File | What it is |
|---|---|
| `engine.js` | A JavaScript port of the C++ algorithms: level sweep (Lemma 1), tail signature, signature, MTTF (Eq. 7), Lévy-frailty kernel (Eq. 20), repair policies, Dinkelbach. Limited to 6 components so every board can be drawn. |
| `page_template.html` | The page (HTML, CSS and UI code). It contains an `/*ENGINE*/` placeholder. |
| `build.py` | Inlines `engine.js` into the template and writes `switchboard-lab.html`. |
| `test_engine.js` | Checks the port against numbers the C++ project reports (MTTF 0.7, 8 minimal path sets, the lethal-shock and binomial-failure-rate ratios, the Fig. 1 repair costs). |

```bash
node test_engine.js     # run from this folder; exit code 1 if a check fails
python3 build.py        # rebuild switchboard-lab.html after editing engine.js or page_template.html
```

This is a teaching tool, not part of the analysis. The C++ library and `analyze` remain the reference, and nothing in the build, the tests or the experiments depends on this folder.
