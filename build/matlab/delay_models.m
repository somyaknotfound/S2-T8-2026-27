% delay_models.m - MATLAB cross-check of the closed-form delay models.
%
% An independent re-implementation of build/retry_correlation_check.py for the base paper's
% tandem (5 caches + origin, synthetic catalogue C = 100, cache size B = 10, tau = 1 ms per link,
% delta = 10 ms, simulator cap of 5000 attempts). It recomputes every analytical value, compares
% it with the value the Python analysis wrote into the summary CSVs, and re-tests the ns-3 means
% against it.
%
% MATLAB Online: run  delay_models  from a folder holding either the repository's results
% folders as they are (stage2/summary.csv, stage3/summary.csv, directly or under results/ or
% research/results/) or these renamed copies, then paste the Command Window output back.
%   stage2_summary.csv   <- research/results/stage2/summary.csv
%   stage3_summary.csv   <- research/results/stage3/summary.csv
% Base MATLAB only (R2019b or later).

P.C = 100; P.B = 10; P.tau = 1.0; P.delta = 10.0; P.m = 5; P.cap = 5000;
idx = 0:P.C-1;
SIM = 1 - abs(idx' - idx) / (P.C - 1);
model = @(policy, alpha, s) expectation(policy, alpha, s, SIM, P);

%% A. The paper's model vs. cache contents that persist across retries (both exact, no cap)
fprintf('\n== A. Paper''s model (contents redrawn every attempt) vs. contents that persist\n');
fprintf('%5s %5s | %10s %12s %8s\n', 'alpha', 's', 'model ms', 'persist ms', 'ratio');
for alpha = [0.1 0.5 0.9]
    for s = [0.90 0.95 1.0]
        a = model("model-uncapped", alpha, s);
        b = model("blind-uncapped", alpha, s);
        fprintf('%5.1f %5.2f | %10.2f %12.2f %8.2f\n', alpha, s, a, b, b / a);
    end
end

%% B. Stage 3 expectations, as in research/results/stage3/model.txt
fprintf('\n== B. Stage 3 expectations (ms), contents persist, blind capped at %d attempts\n', P.cap);
fprintf('%5s %5s | %12s %12s %14s %12s\n', 'alpha', 's', 'blind cap', 'threshold', ...
        'thr persist', 'exclusion');
eq8_gap = 0;
for alpha = [0.1 0.5 0.9 1.0]
    for s = [0.90 0.95 1.0]
        thr = model("threshold", alpha, s);
        thrp = model("threshold-persist", alpha, s);
        eq8_gap = max(eq8_gap, abs(thr - thrp));
        fprintf('%5.1f %5.2f | %12.2f %12.4f %14.4f %12.4f\n', alpha, s, ...
                model("blind", alpha, s), thr, thrp, model("exclusion", alpha, s));
    end
end
fprintf('Eq. (8) closed form vs. exact persistent enumeration: max |diff| = %.2e ms\n', eq8_gap);

%% C. Agreement with the Python values stored in the CSVs
s2 = load_csv('stage2_summary.csv');
s3 = load_csv('stage3_summary.csv');
fprintf('\n== C. MATLAB vs. Python expected_ms\n');
ok = true;

attempt = s2(s2.placement == "per-attempt", :);
d = arrayfun(@(k) model("model", attempt.alpha(k), attempt.threshold(k)), 1:height(attempt))';
gap = max(abs(d - attempt.expected_ms));
ok = ok && gap <= 0.005 + 1e-9;
fprintf('stage2 per-attempt (paper''s model, CSV rounded to 0.01 ms): max |diff| = %.4f ms  %s\n', ...
        gap, verdict(gap <= 0.005 + 1e-9));

request = s2(s2.placement == "per-request", :);
fprintf('stage2 per-request: the CSV holds an 800-configuration Monte Carlo; exact enumeration:\n');
for k = 1:height(request)
    exact = model("blind", request.alpha(k), request.threshold(k));
    fprintf('   alpha %.1f  s %.2f   Monte Carlo %10.2f   exact %10.2f   (%+.1f%%)\n', ...
            request.alpha(k), request.threshold(k), request.expected_ms(k), exact, ...
            100 * (request.expected_ms(k) / exact - 1));
end

for policy = ["blind" "threshold" "exclusion"]
    rows = s3(s3.policy == policy, :);
    d = arrayfun(@(k) model(policy, rows.alpha(k), rows.threshold(k)), 1:height(rows))';
    rel = max(abs(d - rows.expected_ms) ./ rows.expected_ms);
    ok = ok && rel < 1e-9;
    fprintf('stage3 %-9s (%2d cells): max relative diff = %.2e  %s\n', policy, height(rows), ...
            rel, verdict(rel < 1e-9));
end

%% D. ns-3 means vs. the MATLAB model: is the model inside the 95% CI of the simulated mean?
fprintf('\n== D. ns-3 mean within its 95%% CI of the MATLAB expectation\n');
d = arrayfun(@(k) model("model", attempt.alpha(k), attempt.threshold(k)), 1:height(attempt))';
report_ci('stage2 per-attempt', attempt, d);
d = arrayfun(@(k) model("blind", request.alpha(k), request.threshold(k)), 1:height(request))';
report_ci('stage2 per-request', request, d);
for policy = ["blind" "threshold" "exclusion"]
    rows = s3(s3.policy == policy, :);
    d = arrayfun(@(k) model(policy, rows.alpha(k), rows.threshold(k)), 1:height(rows))';
    report_ci("stage3 " + policy, rows, d);
end
fprintf('(for C = 100, s = 0.99 and s = 1.0 are the same cell, so stage 2 counts each twice)\n');

%% E. Threshold mode: the delay falls monotonically in alpha, so alpha = 1 is delay-optimal
fprintf('\n== E. Threshold mode, alpha = 0.05 ... 1\n');
alphas = 0.05:0.05:1;
for s = [0.90 0.95 1.0]
    d = arrayfun(@(a) model("threshold", a, s), alphas);
    falls = all(diff(d) < 0);
    ok = ok && falls;
    fprintf('s = %.2f: %.4f ms at alpha 0.05 -> %.4f ms at alpha 1, strictly decreasing: %s\n', ...
            s, d(1), d(end), verdict(falls));
end

fprintf('\nAll checks: %s\n', verdict(ok));


%% ---------------------------------------------------------------- local functions

function d = expectation(policy, alpha, s, SIM, P)
% Mean delay over the five clients and all C items for one policy.
pAcc = arrayfun(@(c) acceptable_prob(SIM, c, s, P), 1:P.C);
switch policy
    case "model"              % the paper's model: contents redrawn on every attempt
        d = model_mean(pAcc, alpha, P, P.cap);
    case "model-uncapped"
        d = model_mean(pAcc, alpha, P, Inf);
    case "blind"              % blind caches, contents persist
        d = persistent_mean(@(h) blind_delay(h, alpha, P, P.cap), pAcc, P);
    case "blind-uncapped"
        d = persistent_mean(@(h) blind_delay(h, alpha, P, Inf), pAcc, P);
    case "threshold"          % Eq. (8) with q_i = alpha * p_i
        d = threshold_mean(pAcc, alpha, P);
    case "threshold-persist"
        d = persistent_mean(@(h) threshold_delay(h, alpha, P), pAcc, P);
    case "exclusion"
        d = persistent_mean(@(h) exclusion_delay(h, alpha, false(size(h)), P), pAcc, P);
    otherwise
        error('unknown policy %s', policy);
end
end

function p = acceptable_prob(SIM, c, s, P)
% P(a uniform random B-subset of the catalogue holds an item with similarity >= s to item c).
below = sum(SIM(c, :) < s);
if below < P.B
    p = 1;
    return
end
k = 0:P.B-1;
p = 1 - prod((below - k) ./ (P.C - k));
end

function d = expected_delay(tOk, pOk, tBad, pBad, cap, delta)
% Expected delay until an acceptable answer, given per-attempt outcome delays and probabilities.
% With a finite cap the request ends after `cap` failed attempts, at the last rejected answer.
rho = sum(pOk);
dOk = sum(tOk .* pOk) / rho;
fail = sum(pBad);
dBad = 0;
if fail > 0
    dBad = sum(tBad .* pBad) / fail;
end
retry = dBad + delta;
if isinf(cap)
    d = dOk + (1 / rho - 1) * retry;
    return
end
q = 1 - rho;
if q <= 0
    d = dOk;
    return
end
tail = q^cap;
s = q * (1 - cap * q^(cap - 1) + (cap - 1) * q^cap) / (1 - q)^2;   % sum_{n<cap} n q^n
d = dOk * (1 - tail) + retry * rho * s + tail * ((cap - 1) * retry + dBad);
end

function d = model_mean(pAcc, alpha, P, cap)
total = 0;
for c = 1:P.C
    for m = 1:P.m
        i = 0:m-1;
        reach = (1 - alpha).^i;
        tOk = [2 * P.tau * i, 2 * P.tau * m];
        pOk = [reach * alpha * pAcc(c), (1 - alpha)^m];
        pBad = reach * alpha * (1 - pAcc(c));
        total = total + expected_delay(tOk, pOk, 2 * P.tau * i, pBad, cap, P.delta);
    end
end
d = total / (P.C * P.m);
end

function d = blind_delay(hits, alpha, P, cap)
% One fixed configuration: hits(i) = cache i on the path holds an acceptable item.
m = numel(hits);
i = 0:m-1;
reach = (1 - alpha).^i;
tOk = [2 * P.tau * i, 2 * P.tau * m];
pOk = [reach * alpha .* hits, (1 - alpha)^m];
pBad = reach * alpha .* (1 - hits);
if sum(pOk) == 0
    % alpha = 1 and the client's own cache holds nothing acceptable: rejected until the cap
    d = (cap - 1) * P.delta;
    return
end
d = expected_delay(tOk, pOk, 2 * P.tau * i, pBad, cap, P.delta);
end

function d = threshold_delay(hits, alpha, P)
% Threshold-carrying Interest: cache i answers iff it holds an acceptable item and its alpha
% coin says so. Nothing is rejected, so there is no retry term. `hits` may be probabilities.
d = 0;
reach = 1;
for i = 1:numel(hits)
    q = alpha * hits(i);
    d = d + 2 * P.tau * (i - 1) * reach * q;
    reach = reach * (1 - q);
end
d = d + 2 * P.tau * numel(hits) * reach;
end

function d = threshold_mean(pAcc, alpha, P)
total = 0;
for c = 1:P.C
    for m = 1:P.m
        total = total + threshold_delay(repmat(pAcc(c), 1, m), alpha, P);
    end
end
d = total / (P.C * P.m);
end

function d = exclusion_delay(hits, alpha, excluded, P)
% Blind caches; a retry names the caches that served rejected answers, and those forward.
d = 0;
reach = 1;
for i = 1:numel(hits)
    if excluded(i)
        continue
    end
    p = reach * alpha;
    d = d + p * 2 * P.tau * (i - 1);
    if ~hits(i)
        ex = excluded;
        ex(i) = true;
        d = d + p * (P.delta + exclusion_delay(hits, alpha, ex, P));
    end
    reach = reach * (1 - alpha);
end
d = d + reach * 2 * P.tau * numel(hits);
end

function d = persistent_mean(f, pAcc, P)
% Exact mean of f(hits) over clients, items and cache contents. Caches are filled independently,
% so for item c the flags "cache i holds an acceptable item" are i.i.d. Bernoulli(pAcc(c)), and
% every policy depends on a configuration only through them: enumerating the 2^m patterns
% replaces sampling configurations.
total = 0;
for m = 1:P.m
    patterns = dec2bin(0:2^m-1, m) == '1';
    k = sum(patterns, 2);
    fv = zeros(2^m, 1);
    for r = 1:2^m
        fv(r) = f(patterns(r, :));
    end
    for c = 1:P.C
        w = pAcc(c).^k .* (1 - pAcc(c)).^(m - k);
        total = total + w' * fv;
    end
end
d = total / (P.C * P.m);
end

function report_ci(label, rows, expected)
inside = abs(rows.mean_ms - expected) <= rows.ci95_ms;
fprintf('%-20s %2d of %2d cells inside the CI\n', label, sum(inside), height(rows));
end

function T = load_csv(name)
% The summaries write "nan" for missing values and True/False flags; keep numbers numeric.
T = readtable(find_csv(name), 'TextType', 'string', 'Delimiter', ',');
for v = string(T.Properties.VariableNames)
    if isstring(T.(v))
        x = str2double(T.(v));
        if isequal(isnan(x), ismember(lower(T.(v)), ["nan" ""]))
            T.(v) = x;
        end
    end
end
end

function s = verdict(pass)
if pass
    s = 'PASS';
else
    s = 'FAIL';
end
end

function path = find_csv(name)
% 'stageN_<file>.csv' as a renamed upload, or the repository's own layout stageN/<file>.csv,
% directly, under results/ or under research/results/, from the current folder or this
% script's folder. A renamed copy anywhere on the MATLAB path is found too.
parts = split(string(name), '_');
stage = parts(1);
file = strjoin(parts(2:end), '_');
for root = [string(pwd), string(fileparts(mfilename('fullpath')))]
    for sub = ["", "results", fullfile("research", "results")]
        for candidate = [fullfile(root, sub, name), fullfile(root, sub, stage, file)]
            if isfile(candidate)
                path = candidate;
                return
            end
        end
    end
end
onpath = which(name);
if ~isempty(onpath)
    path = string(onpath);
    return
end
error(['Cannot find %s. Upload the repository''s research/results folder with its stage ', ...
       'subfolders (it needs %s/%s), or a copy of that file renamed to %s, then run the ', ...
       'script from the folder that holds it. Current folder: %s'], name, stage, file, name, pwd);
end
