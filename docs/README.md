# Documentation index

Derived from `space_system_reliability_analyzer_project_guide.docx` and validated against `paper3.pdf` (arXiv:2610.22181). Neither file is part of the published repository; keep local copies in this folder if you want the links in the validation report to work.

| Document | Contents |
|---|---|
| [notation-glossary.md](notation-glossary.md) | The paper's symbols in plain language, with examples and the matching code names; pitfalls already met |
| [paper-validation-report.md](paper-validation-report.md) | **Start here.** Findings from checking the guide and these documents against the paper (confirmations, errors fixed, numerical caveats) |
| [requirements-spec.md](requirements-spec.md) | Scope, assumptions, functional and non-functional requirements with acceptance criteria |
| [test-plan.md](test-plan.md) | Test strategy, analytical and paper oracles, test cases A–J, traceability matrix |
| [user-stories.md](user-stories.md) | 26 user stories in 6 epics, with acceptance criteria and links to requirements and tests |
| [technical-report.md](technical-report.md) | **The whole story in one document**: question, method, architecture, reproduced results, experiments, verification, limitations, deviations |
| [file-guide.md](file-guide.md) | **A guided tour of every technical file**: what each one is, what technologies it uses and why, written for a reader who follows the project at a high level (parts 1–2 are a technology primer) |
| [demo.md](demo.md) | A two-minute guided run (`project/demo.sh`) with captured output |
| [sample-output/](sample-output/) | Real captured output of `demo.sh`, the test run, the experiment verdicts and one analysis (JSON) |
| [verification-report.md](verification-report.md) | How the software was verified: environment, results of every build configuration, mutation record, tolerances, defects found, requirement-by-requirement status, what is **not** verified |
| [traceability-matrix.md](traceability-matrix.md) | Requirements → test cases → test code, generated and machine-checked by `project/tools/check_traceability.py` |
| [deviation-log.md](deviation-log.md) | Every deviation from the paper and the guide, every correction, decision and limitation |
| [experiment-report.md](experiment-report.md) | Results of Experiments 1–7 with figures: what reproduces the paper, what is new, every prediction supported or **rejected**, limitations and deviations |
| [project-plan.md](project-plan.md) | Ten-day plan with daily tasks, quality gates, risks, definition of done and decisions needed |
| [conops.md](conops.md) | Concept of operations (NASA SEH App. S outline): needs, modes, nominal and off-nominal scenarios, risks |
| [software-design-description.md](software-design-description.md) | Design decisions, architecture and per-module design (DI-IPSC-81435A, tailored), with requirement traceability |
| [interface-design-description.md](interface-design-description.md) | Every interface: config file, command line and exit codes, result JSON, library API, Python→CLI contract, experiment files (DI-IPSC-81436A, tailored) |
| [decision-log.md](decision-log.md) | Nine architecture decision records (MADR 4.0.0), written retrospectively from the history |
| [configuration-management-plan.md](configuration-management-plan.md) | Configuration items, baselines, change control, audits and CM gaps (NASA SEH App. M topics) |
| [version-description.md](version-description.md) | What version 0.8.0 contains, what changed, how to install, known errors (DI-IPSC-81442A, tailored) |
| Use case diagram | [PNG](uml/use-case.png) · [source](uml/use-case.puml) |
| Sequence diagram: analyse, verify and run an experiment | [PNG](uml/sequence.png) · [source](uml/sequence.puml) |
| Component diagram | [PNG](uml/component.png) · [source](uml/component.puml) |
| Object diagram: satellite-support reference system snapshot | [PNG](uml/object.png) · [source](uml/object.puml) |
| SysML block definition diagram: structure of the analyzer (blocks, parts, values, constraints) | [PNG](uml/sysml-bdd.png) · [SVG](uml/sysml-bdd.svg) · [source](uml/sysml_bdd.py) |

The code lives in [../project/](../project/); the repository front page is [../README.md](../README.md).

## Rendering the UML

The PNGs in `uml/` were rendered locally with PlantUML 1.2024.7 using the pure-Java Smetana layout engine (`!pragma layout smetana`, so Graphviz is not needed). Smetana lays out the use-case and component diagrams slightly differently from Graphviz. To re-render with a standard install:

```bash
plantuml -tpng docs/uml/*.puml
```

You can also paste a `.puml` file into <https://www.plantuml.com/plantuml> or use the PlantUML extension in VS Code.

## Status

Last reviewed 2026-11-03. The folder holds the requirements, design, test and verification documents, the reports, the decision log, the configuration-management plan, the version description (0.8.0) and the diagrams listed above. `project/run_all.sh` was re-run on 2026-11-03: 129 / 129 tests passed and the experiment verdicts match [experiment-report.md](experiment-report.md) (see [verification-report.md](verification-report.md) §10).

The documents were validated against `paper3.pdf`: the core equations (Eqs. (1), (2), (5)–(8), (19)–(20), (22)) and the worked numbers were re-computed independently and match; the paper's Fig. 1 example was reproduced exactly. One incorrect test expectation and one outdated diagram were corrected. See [paper-validation-report.md](paper-validation-report.md). Eq. (21) was checked against the typeset PDF on 2026-10-21 and the paper has a typo in its lower limit (§6 of the report).

History: the first five documents were rebuilt on 2026-10-20 after the original `docs/` folder disappeared from disk; the UML sources and PNGs were restored from the render copies. The set has grown considerably since.
