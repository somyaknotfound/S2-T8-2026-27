## Project
CS301 Computer Networks mini-project (NITK, Prof. B. R. Chandavarkar), team T8: Somyak (241CS257), Pranav Shaji, Chris Tony.
**Threshold-Aware Similarity Caching for Named Data Networks: Bounding Retry Delay under Congestion using ns-3.**
Base paper: Nakamura & Kamiyama, IEEE Access 2024 (`research/documents/`). Proposal: `Proposal_255_257.tex`.
Deadline: about four weeks from 5 Oct 2026. Scope is fixed. Don't add features nobody asked for.

## What we are building
In ns-3 + ndnSIM:
1. A similarity Content Store — best match by similarity, not exact name.
2. Consumer apps that check similarity and retry after δ.
3. A producer (origin).
4. An answer-or-forward forwarding strategy with three modes:
   - `blind` — the paper: answer with probability α;
   - `threshold` — answer only if the best match ≥ s_min carried in the Interest, with bounded retries;
   - `delay-aware` — answer if s' ≥ F⁻¹(1 − D_up/(δ + D_retry)), with D_up from an SRTT-style EWMA.
5. Scenarios: the paper's tandem (5 caches + origin, 1 ms links); bandwidth-limited links with cross-traffic; Gilbert–Elliott burst loss (a custom ns-3 ErrorModel).

## Results we must produce, in order
1. Reproduce the paper. The corrected analytical model is in `build/retry_correlation_check.py`; its numbers are in `research/results/retry_correlation.txt`. Simulation must match the "model" column within confidence intervals when cache contents are re-randomised per attempt.
2. Persistent cache contents give a heavy-tailed delay, matching the "persist" column.
3. `threshold` mode removes the tail.
4. `delay-aware` mode beats every fixed α under congestion and loss.

## Layout (set by the user — keep it)
- `build/` — all code. ndnSIM scenarios live in `build/scenario/`, copied from named-data-ndnSIM/scenario-template.
- `submission/` — all `.tex` for submission (`main.tex` is the progress report; layout cloned from last semester).
- `research/documents/` — papers.
- `research/context/` — design and discussion docs.
- `research/results/` — outputs: CSV plus a config/seed note per run.
- ndnSIM itself lives OUTSIDE the repo, at `/root/ndnSIM` inside the container. Never commit it.
- Current status and next steps: `research/context/STATUS.md`. Read it at the start of a session and update it at the end.

## Environment
Host: a Kali Linux VM running Claude Code and Antigravity. Kali is rolling Debian, and its gcc/Boost are too new for ndnSIM (docs list Ubuntu 20.04/21.10 only).
**So ndnSIM builds and runs inside an `ubuntu:20.04` Docker container, with this repo mounted at `/work`:**
```
sudo apt install -y docker.io && sudo usermod -aG docker $USER      # once, then log out and back in
docker run -dit --name ndnsim -v "$PWD":/work -w /work ubuntu:20.04 bash   # run from the repo root
docker exec ndnsim bash -lc '<command>'                                    # how Claude runs things inside
```
Edit code on the host (it's the same files via the mount); build and run with `docker exec`. If the container is gone, `docker start ndnsim`.
Inside the container (ndnSIM at `/root/ndnSIM`, outside `/work`):
```
apt update && DEBIAN_FRONTEND=noninteractive apt install -y build-essential libsqlite3-dev libboost-all-dev libssl-dev git pkg-config python3 python3-setuptools castxml
mkdir ~/ndnSIM && cd ~/ndnSIM
git clone https://github.com/named-data-ndnSIM/ns-3-dev.git ns-3
git clone https://github.com/named-data-ndnSIM/pybindgen.git pybindgen
git clone --recursive https://github.com/named-data-ndnSIM/ndnSIM.git ns-3/src/ndnSIM
cd ns-3 && ./waf configure --disable-python --enable-examples && ./waf && ./waf --run=ndn-simple
```
The scenario template needs ndnSIM installed: run `./waf install && ldconfig` in `/root/ndnSIM/ns-3` (already root in the container, no sudo), then follow the template's README.

## Open questions — answer these in week 1 by reading the code, then record the answers in `research/context/design_notes.md`
- **Where does similarity lookup live?** Options: NFD's `nfd::cs::Cs`; ndnSIM's old `ns3::ndn::ContentStore` (`StackHelper::SetOldContentStore`); or the strategy plus a side index. Pick the one needing the least forwarder surgery.
- **NDN name matching.** A Data packet must match its Interest's name, so a substitute item can't be returned as-is. Decide between encapsulation (Data under the query name, with the substitute's name and similarity in the payload) and changing matching in our fork. Use Interest ApplicationParameters for s_min if this ndn-cxx version has them.
- **Do NDN packets pass through ns-3's TrafficControlLayer (RED/CoDel)?** They are likely to go straight to the NetDevice queue. If so, congestion experiments use NetDevice queue sizes, and the report must not claim AQM results.

## Rules
- Never cite a paper that hasn't been verified (arXiv API / Crossref). Use the `citation-checker` agent.
- Every result file records ndnSIM commit, scenario, all parameters and RNG seed. Run ≥5 seeds; report means with 95% CIs.
- Keep raw traces under ~10 MB in git. Larger ones go to `research/results/raw/` (gitignored).
- Ask before `git commit`. Never `git push` without explicit approval.
- C++: follow ndnSIM's style in the file you edit. Build with no new warnings.
