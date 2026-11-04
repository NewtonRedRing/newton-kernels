# NewtonKernels GitHub Pages site

The static website in this directory has a **benchmark dashboard** at `index.html`
and a separate **documentation portal** at `docs/index.html`. It uses HTML,
CSS and JavaScript without a framework, build-time package installation or
external runtime dependency.

## Preview locally

```sh
python3 tools/build_showcase.py --check
python3 tools/check_docs.py
python3 -m http.server 8000 --directory showcase
```

Open `http://localhost:8000/` for the dashboard or
`http://localhost:8000/docs/` for the documentation.

## Publishing

`.github/workflows/showcase-pages.yml` packages the **entire** `showcase/`
tree after verifying evidence and local links. Commits to `showcase/**`,
`docs/**`, and the listed generator/workflow files trigger publication from
`main`; manual workflow dispatch is also available.

A repository owner must configure **Settings → Pages → Build and deployment →
Source: GitHub Actions**. The workflow then deploys to the GitHub Pages URL
reported in the action output. The expected URL is
`https://newtonredring.github.io/newton-kernels/`, but do not assert it is
live unless deployment succeeds and the URL responds.

The deploy job uploads a downloadable preview artifact even if GitHub Pages
has not been enabled. The normal `GITHUB_TOKEN` cannot initialize a new
Pages site on its own.

## Evidence provenance

`showcase/data/results.json` and `extended-results.json` are **intentional
committed copies** of immutable benchmark evidence. They are checked
byte-for-byte against the source snapshots by
`python3 tools/build_showcase.py --check`. Deleting or hand-editing these
files breaks this provenance contract.

Published results include competitor wins, not only NewtonKernels wins.
Peak RSS is a whole-process measure, not per-library memory usage.
Benchmark campaigns on different machines must not be ranked by absolute
times. No results or dashboard numbers are fabricated.
