% report_figures.m - the report's result figures, redrawn in MATLAB from the committed summaries.
%
% MATLAB Online: upload these files next to this script (rename as shown), then run
% report_figures. Vector PDFs and 200-dpi PNGs go to ./matlab_figures/; copy that folder's
% contents to submission/figures/matlab/ in the repo.
%   stage2_summary.csv   <- research/results/stage2/summary.csv
%   stage3_summary.csv   <- research/results/stage3/summary.csv
%   stage4_summary.csv   <- research/results/stage4/summary.csv
%   stage4_nodes.csv     <- research/results/stage4/nodes.csv
% Base MATLAB only (R2020a or later, for exportgraphics).
%
% Colours: one fixed hue per policy in every figure (delay-aware blue, blind orange,
% threshold/oracle aqua, exclusion violet); ordered series (alpha, s, cache position) use a
% light-to-dark ramp of their policy's hue. All sets pass a colour-vision-deficiency check.
% Aqua is below 3:1 contrast on white, so it always also has its own marker and line style.

OUT = 'matlab_figures';
if ~exist(OUT, 'dir')
    mkdir(OUT);
end
K = palette();

s2 = load_csv('stage2_summary.csv');
s3 = load_csv('stage3_summary.csv');
s4 = load_csv('stage4_summary.csv');
nodes = load_csv('stage4_nodes.csv');

fig_stage2_delay(s2, K, OUT);
fig_stage3_policies(s3, K, OUT);
fig_stage3_threshold_alpha(s3, K, OUT);
fig_stage3_hops(s3, K, OUT);
fig_stage4_delay(s4(s4.scenario == "load", :), K, OUT, 'stage4_load', ...
                 'Offered cross-traffic load on the backhaul', ...
                 'Mixed users, congestion on the backhaul');
fig_stage4_delay(s4(s4.scenario == "loss", :), K, OUT, 'stage4_loss', ...
                 'Mean loss per link (%)', ...
                 'Mixed users, Gilbert-Elliott burst loss on every link');
fig_stage4_learned(nodes, K, OUT);
fig_stage4_cross(s4(s4.scenario == "load", :), K, OUT);
fprintf('Wrote 8 figures (PDF + PNG) to %s/\n', OUT);


%% ---------------------------------------------------------------- figures

function fig_stage2_delay(T, K, OUT)
% Simulated mean delay vs. required similarity, with the analytical value dashed.
T = T(T.threshold ~= 0.99, :);   % for C = 100, s = 0.99 and s = 1 are the same cell
alphas = [0.1 0.5 0.9];
placements = ["per-attempt" "per-request"];
titles = ["Contents redrawn every attempt (paper's model)", ...
          "Contents persist across retries (real caches)"];
[fig, tl] = new_figure(10, 4, 2);
axs = gobjects(1, 2);
for k = 1:2
    ax = nexttile(tl);
    hold(ax, 'on');
    for j = 1:numel(alphas)
        R = sortrows(T(T.placement == placements(k) & near(T.alpha, alphas(j)), :), 'threshold');
        c = K.blindRamp(j, :);
        errorbar(ax, R.threshold, R.mean_ms, lower_bar(R), R.ci95_ms, 'o', 'Color', c, ...
                 'MarkerFaceColor', c, 'MarkerSize', 5, 'LineWidth', 1, 'CapSize', 4, ...
                 'DisplayName', sprintf('\\alpha = %.1f (ns-3)', alphas(j)));
        plot(ax, R.threshold, R.expected_ms, '--', 'Color', c, 'LineWidth', 1, ...
             'HandleVisibility', 'off');
    end
    style_axes(ax, K, 'log');
    xlim(ax, [0.885 1.015]);
    xticks(ax, [0.9 0.95 1]);
    xlabel(ax, 'Required similarity s');
    title(ax, titles(k), 'FontWeight', 'normal', 'FontSize', 9);
    axs(k) = ax;
end
linkaxes(axs, 'y');
ylabel(axs(1), 'Mean similar-content delivery delay (ms)');
lg = legend(axs(1), 'Location', 'northwest', 'FontSize', 8);
title(lg, 'dashed = analytical', 'FontWeight', 'normal');
save_figure(fig, OUT, 'stage2_delay');
end

function fig_stage3_policies(T, K, OUT)
% Mean (with analytical value dashed) and p99 delay vs. s, alpha = 0.9, contents persist.
[fig, tl] = new_figure(10, 4, 2);
axMean = nexttile(tl);
hold(axMean, 'on');
axTail = nexttile(tl);
hold(axTail, 'on');
for policy = ["blind" "exclusion" "threshold"]
    R = sortrows(T(T.policy == policy & near(T.alpha, 0.9) & T.placement == "per-request", :), ...
                 'threshold');
    st = K.(policy);
    errorbar(axMean, R.threshold, R.mean_ms, lower_bar(R), R.ci95_ms, st.marker, ...
             'Color', st.color, 'MarkerFaceColor', st.color, 'MarkerSize', 5, 'LineWidth', 1, ...
             'CapSize', 4, 'DisplayName', policy + " (ns-3)");
    plot(axMean, R.threshold, R.expected_ms, '--', 'Color', st.color, 'LineWidth', 1, ...
         'HandleVisibility', 'off');
    plot(axTail, R.threshold, R.p99_ms, [st.marker st.line], 'Color', st.color, ...
         'MarkerFaceColor', st.color, 'MarkerSize', 5, 'LineWidth', 1.5, 'DisplayName', policy);
end
names = ["Mean delay (dashed = analytical)" "99th-percentile delay"];
axs = [axMean axTail];
for k = 1:2
    style_axes(axs(k), K, 'log');
    xlim(axs(k), [0.885 1.015]);
    xticks(axs(k), [0.9 0.95 1]);
    xlabel(axs(k), 'Required similarity s');
    title(axs(k), names(k), 'FontWeight', 'normal', 'FontSize', 9);
end
linkaxes(axs, 'y');
ylabel(axMean, 'Similar-content delivery delay (ms)');
legend(axMean, 'Location', 'northwest', 'FontSize', 8);
title(tl, '\alpha = 0.9, cache contents persist across retries', 'FontSize', 10);
save_figure(fig, OUT, 'stage3_policies');
end

function fig_stage3_threshold_alpha(T, K, OUT)
% Threshold mode: delay falls monotonically in alpha, so alpha = 1 is delay-optimal.
[fig, tl] = new_figure(5.5, 4, 1);
ax = nexttile(tl);
hold(ax, 'on');
S = [0.9 0.95 1.0];
grid_alpha = linspace(0.05, 1, 40);
for j = 1:numel(S)
    c = K.thresholdRamp(j, :);
    for placement = ["per-request" "static"]
        R = sortrows(T(T.policy == "threshold" & near(T.threshold, S(j)) & ...
                       T.placement == placement, :), 'alpha');
        if placement == "per-request"
            shift = -0.012; marker = 'o'; face = c;
        else
            shift = 0.012; marker = 's'; face = 'none';
        end
        errorbar(ax, R.alpha + shift, R.mean_ms, R.ci95_ms, marker, 'Color', c, ...
                 'MarkerFaceColor', face, 'MarkerSize', 5, 'LineWidth', 1, 'CapSize', 3, ...
                 'DisplayName', sprintf('s = %g, %s', S(j), placement));
    end
    plot(ax, grid_alpha, arrayfun(@(a) threshold_closed_form(a, S(j)), grid_alpha), '--', ...
         'Color', c, 'LineWidth', 1, 'HandleVisibility', 'off');
end
style_axes(ax, K, 'linear');
xlabel(ax, 'Answer probability \alpha');
ylabel(ax, 'Mean delay, threshold mode (ms)');
title(ax, {'Threshold mode: delay falls monotonically to \alpha = 1', ...
           '(dashed = Eq. 8, exact)'}, 'FontWeight', 'normal', 'FontSize', 9);
legend(ax, 'Location', 'northeast', 'FontSize', 7);
save_figure(fig, OUT, 'stage3_threshold_alpha');
end

function fig_stage3_hops(T, K, OUT)
% Upstream Interest hops per request: the bandwidth cost of each policy.
[fig, tl] = new_figure(6, 4, 1);
ax = nexttile(tl);
policies = ["blind" "exclusion" "threshold"];
S = [0.9 0.95 1.0];
H = zeros(numel(S), numel(policies));
for i = 1:numel(policies)
    R = T(T.policy == policies(i) & near(T.alpha, 0.9) & T.placement == "per-request", :);
    for j = 1:numel(S)
        H(j, i) = R.interest_hops(near(R.threshold, S(j)));
    end
end
b = bar(ax, H, 'grouped', 'BaseValue', 0.1, 'EdgeColor', 'w', 'LineWidth', 1);
for i = 1:numel(policies)
    b(i).FaceColor = K.(policies(i)).color;
    b(i).DisplayName = policies(i);
    text(ax, b(i).XEndPoints, b(i).YEndPoints, compose('%.2f', H(:, i)), ...
         'HorizontalAlignment', 'center', 'VerticalAlignment', 'bottom', 'FontSize', 7, ...
         'Color', K.ink);
end
style_axes(ax, K, 'log');
ax.XGrid = 'off';
xticks(ax, 1:numel(S));
xticklabels(ax, compose('s = %g', S));
ylim(ax, [0.1 1000]);
ylabel(ax, 'Upstream Interest hops per request');
title(ax, 'Bandwidth cost per request (\alpha = 0.9, contents persist)', ...
      'FontWeight', 'normal', 'FontSize', 9);
legend(ax, 'Location', 'northwest', 'FontSize', 8);
save_figure(fig, OUT, 'stage3_hops');
end

function fig_stage4_delay(T, K, OUT, name, xname, heading)
% Every fixed-alpha exclusion cache against delay-aware, the oracle and the paper's blind cache.
[fig, tl] = new_figure(8.5, 4.2, 1);
ax = nexttile(tl);
hold(ax, 'on');
for i = 1:numel(K.alphas)
    R = policy_rows(T, sprintf('exclusion-a%.1f', K.alphas(i)));
    errorbar(ax, R.x, R.mean_ms, lower_bar(R), R.ci95_ms, '.-', 'Color', K.exclusionRamp(i, :), ...
             'MarkerSize', 10, 'LineWidth', 1, 'CapSize', 2, ...
             'DisplayName', sprintf('exclusion \\alpha = %.1f', K.alphas(i)));
end
for key = ["delay_aware" "threshold" "blind"]
    st = K.(key);
    R = policy_rows(T, st.policy);
    errorbar(ax, R.x, R.mean_ms, lower_bar(R), R.ci95_ms, [st.marker st.line], ...
             'Color', st.color, 'MarkerFaceColor', st.color, 'MarkerSize', 5, ...
             'LineWidth', st.width, 'CapSize', 3, 'DisplayName', st.name);
end
style_axes(ax, K, 'log');
xlabel(ax, xname);
ylabel(ax, 'Mean delivery delay after warm-up (ms)');
title(ax, heading, 'FontWeight', 'normal', 'FontSize', 9);
legend(ax, 'Location', 'eastoutside', 'FontSize', 7);
save_figure(fig, OUT, name);
end

function fig_stage4_learned(N, K, OUT)
% Learned state of the delay-aware caches: upstream delay and threshold vs. load, per position.
[fig, tl] = new_figure(10, 4, 2);
axUp = nexttile(tl);
hold(axUp, 'on');
axS = nexttile(tl);
hold(axS, 'on');
positions = unique(N.position)';
for p = positions
    R = sortrows(N(N.position == p, :), 'offered_load');
    c = K.positionRamp(p + 1, :);
    label = sprintf('v%d', p + 1);
    plot(axUp, R.offered_load, R.upstream_ms, 'o-', 'Color', c, 'MarkerFaceColor', c, ...
         'MarkerSize', 5, 'LineWidth', 1.5, 'DisplayName', label);
    plot(axS, R.offered_load, R.threshold_own, 'o-', 'Color', c, 'MarkerFaceColor', c, ...
         'MarkerSize', 5, 'LineWidth', 1.5, 'DisplayName', label);
end
ylabel(axUp, 'Measured upstream delay D_{up} (ms)');
ylabel(axS, 'Learned threshold s^* for its own client');
for ax = [axUp axS]
    style_axes(ax, K, 'linear');
    xlabel(ax, 'Offered cross-traffic load on the backhaul');
    lg = legend(ax, 'Location', 'best', 'FontSize', 8);
    title(lg, 'cache', 'FontWeight', 'normal');
end
title(tl, 'Delay-aware caches: congestion raises D_{up}, so caches answer more', 'FontSize', 10);
save_figure(fig, OUT, 'stage4_learned');
end

function fig_stage4_cross(T, K, OUT)
% Externality: mean delay of the cross traffic sharing the backhaul, per policy.
[fig, tl] = new_figure(8.5, 4.2, 1);
ax = nexttile(tl);
hold(ax, 'on');
for i = 1:numel(K.alphas)
    R = policy_rows(T, sprintf('exclusion-a%.1f', K.alphas(i)));
    plot(ax, R.x, R.cross_delay_ms, '.-', 'Color', K.exclusionRamp(i, :), 'MarkerSize', 10, ...
         'LineWidth', 1, 'DisplayName', sprintf('exclusion \\alpha = %.1f', K.alphas(i)));
end
for key = ["delay_aware" "threshold" "blind"]
    st = K.(key);
    R = policy_rows(T, st.policy);
    plot(ax, R.x, R.cross_delay_ms, [st.marker st.line], 'Color', st.color, ...
         'MarkerFaceColor', st.color, 'MarkerSize', 5, 'LineWidth', st.width, ...
         'DisplayName', st.name);
end
style_axes(ax, K, 'linear');
xlabel(ax, 'Offered cross-traffic load on the backhaul');
ylabel(ax, 'Cross-traffic mean delay (ms)');
title(ax, 'Externality: what each policy does to other traffic on the backhaul', ...
      'FontWeight', 'normal', 'FontSize', 9);
legend(ax, 'Location', 'eastoutside', 'FontSize', 7);
save_figure(fig, OUT, 'stage4_cross');
end


%% ---------------------------------------------------------------- helpers

function K = palette()
rgb = @(hex) reshape(sscanf(hex(2:end), '%2x'), 1, 3) / 255;
ramp = @(hexes) cell2mat(cellfun(rgb, hexes', 'UniformOutput', false));
K.ink = rgb('#0b0b0b');
K.ink2 = rgb('#52514e');
K.grid = rgb('#e1e0d9');
K.delay_aware = struct('policy', 'delay-aware', 'name', 'delay-aware (ours)', ...
                       'color', rgb('#2a78d6'), 'marker', 'o', 'line', '-', 'width', 2.2);
K.blind = struct('policy', 'blind-a0.5', 'name', 'blind \alpha = 0.5 (paper)', ...
                 'color', rgb('#eb6834'), 'marker', '^', 'line', ':', 'width', 1.5);
K.threshold = struct('policy', 'threshold-a1', 'name', 'oracle: s_{min} in Interest', ...
                     'color', rgb('#1baf7a'), 'marker', 's', 'line', '--', 'width', 1.5);
K.exclusion = struct('policy', '', 'name', 'exclusion', ...
                     'color', rgb('#4a3aa7'), 'marker', 'd', 'line', '-', 'width', 1.5);
K.alphas = [0.1 0.3 0.5 0.7 0.9 1.0];
K.exclusionRamp = ramp({'#a99ff0' '#8a7de6' '#6f5fd6' '#5a49c2' '#4a3aa7' '#33267f'});
K.blindRamp = ramp({'#ec7a49' '#c4501c' '#7f3110'});
K.thresholdRamp = ramp({'#1baf7a' '#118a5e' '#0a5e40'});
K.positionRamp = ramp({'#86b6ef' '#5598e7' '#2a78d6' '#1c5cab' '#104281'});
end

function [fig, tl] = new_figure(width, height, tiles)
fig = figure('Color', 'w', 'WindowStyle', 'normal', 'Units', 'inches', ...
             'Position', [0.5 0.5 width height]);
tl = tiledlayout(fig, 1, tiles, 'TileSpacing', 'compact', 'Padding', 'compact');
end

function style_axes(ax, K, yscale)
ax.YScale = yscale;
ax.FontSize = 9;
ax.Box = 'off';
ax.TickDir = 'out';
ax.XColor = K.ink2;
ax.YColor = K.ink2;
ax.GridColor = K.grid;
ax.GridAlpha = 1;
ax.MinorGridColor = K.grid;
ax.MinorGridAlpha = 0.5;
ax.XGrid = 'on';
ax.YGrid = 'on';
ax.YMinorGrid = strcmp(yscale, 'log');
ax.Layer = 'bottom';
end

function save_figure(fig, OUT, name)
exportgraphics(fig, fullfile(OUT, [name '.pdf']), 'ContentType', 'vector');
exportgraphics(fig, fullfile(OUT, [name '.png']), 'Resolution', 200);
end

function R = policy_rows(T, policy)
R = sortrows(T(T.policy == policy, :), 'x');
end

function e = lower_bar(R)
% On a log axis a whisker below zero cannot be drawn: keep it above the axis.
e = min(R.ci95_ms, 0.95 * R.mean_ms);
end

function tf = near(x, v)
tf = abs(x - v) < 1e-9;
end

function d = threshold_closed_form(alpha, s)
% Eq. (8) with q_i = alpha * p_i, averaged over the five clients and all C items
% (the same model as delay_models.m: C = 100, B = 10, tau = 1 ms, five caches).
C = 100; B = 10; tau = 1; m = 5;
idx = 0:C-1;
total = 0;
for c = 1:C
    below = sum(1 - abs(idx - (c - 1)) / (C - 1) < s);
    p = 1;
    if below >= B
        p = 1 - prod((below - (0:B-1)) ./ (C - (0:B-1)));
    end
    for len = 1:m
        reach = 1;
        for i = 1:len
            total = total + 2 * tau * (i - 1) * reach * alpha * p;
            reach = reach * (1 - alpha * p);
        end
        total = total + 2 * tau * len * reach;
    end
end
d = total / (C * m);
end

function T = load_csv(name)
% The summaries write "nan" for missing values and True/False flags; keep numbers numeric.
T = readtable(name, 'TextType', 'string', 'Delimiter', ',');
for v = string(T.Properties.VariableNames)
    if isstring(T.(v))
        x = str2double(T.(v));
        if isequal(isnan(x), ismember(lower(T.(v)), ["nan" ""]))
            T.(v) = x;
        end
    end
end
end
