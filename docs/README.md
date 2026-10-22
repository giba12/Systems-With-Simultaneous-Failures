# Documentation index

Derived from `space_system_reliability_analyzer_project_guide.docx` and validated against `paper3.pdf`.

| Document | Contents |
|---|---|
| [notation-glossary.md](notation-glossary.md) | The paper's symbols in plain language, with examples and the matching code names; pitfalls already met |
| [paper-validation-report.md](paper-validation-report.md) | **Start here.** Findings from checking the guide and these documents against the paper (confirmations, errors fixed, numerical caveats) |
| [requirements-spec.md](requirements-spec.md) | Scope, assumptions, functional and non-functional requirements with acceptance criteria |
| [test-plan.md](test-plan.md) | Test strategy, analytical and paper oracles, test cases A–J, traceability matrix |
| [user-stories.md](user-stories.md) | 26 user stories in 6 epics, with acceptance criteria and links to requirements and tests |
| [experiment-report.md](experiment-report.md) | Results of Experiments 1–4 with figures: what reproduces the paper, what is new, every prediction supported or **rejected**, limitations and deviations |
| [project-plan.md](project-plan.md) | Ten-day plan with daily tasks, quality gates, risks, definition of done and decisions needed |
| Use case diagram | [PNG](uml/use-case.png) · [source](uml/use-case.puml) |
| Sequence diagram: analyse, verify and run an experiment | [PNG](uml/sequence.png) · [source](uml/sequence.puml) |
| Component diagram | [PNG](uml/component.png) · [source](uml/component.puml) |
| Object diagram: satellite-support reference system snapshot | [PNG](uml/object.png) · [source](uml/object.puml) |

The C++ project skeleton lives in [../project/](../project/).

## Rendering the UML

The PNGs in `uml/` were rendered locally with PlantUML 1.2024.7 using the pure-Java Smetana layout engine (`!pragma layout smetana`, so Graphviz is not needed). Smetana lays out the use-case and component diagrams slightly differently from Graphviz. To re-render with a standard install:

```bash
plantuml -tpng docs/uml/*.puml
```

You can also paste a `.puml` file into <https://www.plantuml.com/plantuml> or use the PlantUML extension in VS Code.

## Status

Version 1.1 (2026-10-20). The documents were validated against `paper3.pdf`: the core equations (Eqs. (1), (2), (5)–(8), (19)–(20), (22)) and the worked numbers were re-computed independently and match; the paper's Fig. 1 example was reproduced exactly. One incorrect test expectation and one outdated diagram were corrected. See [paper-validation-report.md](paper-validation-report.md). Eq. (21) was checked against the typeset PDF on 2026-10-21 and the paper has a typo in its lower limit (§6 of the report).

These documents were rebuilt on 2026-10-20 after the original `docs/` folder disappeared from disk. The UML sources and PNGs were restored from the render copies (file sizes match the originals); the five documents were reconstructed from my earlier work and may differ in small wording details.
