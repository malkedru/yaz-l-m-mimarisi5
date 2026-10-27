# v5 Verification

Verified on Windows with MSVC using:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Test
```

| Test program | Passed |
| --- | ---: |
| CommandLineTests | 20/20 |
| ErrorHandlingTests | 161/161 |
| FileSinkTests | 12/12 |
| ParserStageTests | 55/55 |
| PipelineBuilderTests | 28/28 |
| PipelineLoaderTests | 45/45 |
| PredicateFilterStageTests | 15/15 |
| ProjectionStageTests | 20/20 |
| IntegrationTest | 11/11 |
| Total | 367/367 |

Executable smoke checks: --version reports LogFlow v5; dirty-dead-letter configuration completes with received=11, emitted=3, dropped=0, deadLettered=8, aborted=0, reconciled=true. Legacy CLI reports received=10, emitted=1, dropped=9. Empty input, skip and fail-fast paths are covered by ErrorHandlingTests. Existing v4 examples retain exact output checks in PipelineLoaderTests.

Outputs use overwrite semantics. Regenerate the demonstration after running all tests:

```powershell
.\build\logflow.exe --config configs/dirty-dead-letter.json
```
