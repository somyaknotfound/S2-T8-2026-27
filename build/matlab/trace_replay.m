% trace_replay.m - the delay-aware caches as a run unfolds: a convergence figure and a replay.
%
% MATLAB Online: upload these files next to this script (rename as shown), then run trace_replay.
%   stage4_trace_delay.csv   <- research/results/stage4/trace_delay.csv
%   stage4_trace_nodes.csv   <- research/results/stage4/trace_nodes.csv
% It writes to ./matlab_figures/ (copy the contents to submission/figures/matlab/):
%   stage4_convergence.pdf/.png   three loads, mean over the five runs
%   stage4_replay.gif             ONE run played back, 100 requests per frame
% Base MATLAB only (R2020a or later).
%
% Each trace row covers 100 requests of one run. The delay rows hold the mean delay of those
% requests; the node rows hold each cache's upstream delay D_up and learned threshold s* at the
% end of them. The first 500 requests of a run are warm-up and are excluded from every result.

LOAD = 0.95;   % offered backhaul load to replay: 0, 0.3, 0.6, 0.8, 0.9 or 0.95
RUN = 1;       % run to replay: 1 to 5
WARMUP = 500;
OUT = 'matlab_figures';
if ~exist(OUT, 'dir')
    mkdir(OUT);
end
K = palette();

D = load_csv('stage4_trace_delay.csv');
N = load_csv('stage4_trace_nodes.csv');

fig_convergence(D, N, K, OUT, [0 0.9 0.95], WARMUP);
replay(D, N, K, OUT, LOAD, RUN, WARMUP);
fprintf('Wrote stage4_convergence (PDF + PNG) and stage4_replay.gif to %s/\n', OUT);

% How often a cache is in "answer whatever it holds" mode, as in convergence.md
fprintf('\nShare of snapshots after warm-up with s* below 0.5 (all runs):\n');
fprintf('%6s %6s %6s %6s %6s %6s\n', 'load', 'v1', 'v2', 'v3', 'v4', 'v5');
for x = unique(N.offered_load)'
    share = zeros(1, 5);
    for p = 0:4
        s = N.threshold_own(near(N.offered_load, x) & N.position == p & N.request > WARMUP);
        share(p + 1) = 100 * mean(s < 0.5);
    end
    fprintf('%6.2f %5.0f%% %5.0f%% %5.0f%% %5.0f%% %5.0f%%\n', x, share);
end


%% ---------------------------------------------------------------- figures

function fig_convergence(D, N, K, OUT, loads, WARMUP)
% Top: mean delay per 100 requests with a 95% CI band over the runs. Bottom: mean s* per cache.
[fig, tl] = new_figure(13, 6.4, 2, numel(loads));
for col = 1:numel(loads)
    x = loads(col);
    ax = nexttile(tl, col);
    hold(ax, 'on');
    shown = series_styles(K, best_fixed(D, x, WARMUP));
    for i = 1:numel(shown)
        R = D(near(D.offered_load, x) & D.policy == shown(i).policy, :);
        G = groupsummary(R, 'request', {'mean', 'std'}, 'mean_ms');
        half = t975(G.GroupCount - 1) .* G.std_mean_ms ./ sqrt(G.GroupCount);
        fill(ax, [G.request; flipud(G.request)], ...
             [G.mean_mean_ms - half; flipud(G.mean_mean_ms + half)], shown(i).color, ...
             'FaceAlpha', 0.15, 'EdgeColor', 'none', 'HandleVisibility', 'off');
        plot(ax, G.request, G.mean_mean_ms, shown(i).line, 'Color', shown(i).color, ...
             'LineWidth', shown(i).width, 'DisplayName', shown(i).name);
    end
    style_axes(ax, K);
    shade_warmup(ax, K, WARMUP);
    title(ax, sprintf('Offered backhaul load %g', x), 'FontWeight', 'normal', 'FontSize', 9);
    ylabel(ax, 'Mean delay per 100 requests (ms)');
    legend(ax, 'Location', 'northeast', 'FontSize', 7);

    ax = nexttile(tl, numel(loads) + col);
    hold(ax, 'on');
    for p = 0:4
        R = N(near(N.offered_load, x) & N.position == p, :);
        G = groupsummary(R, 'request', 'mean', 'threshold_own');
        c = K.positionRamp(p + 1, :);
        plot(ax, G.request, G.mean_threshold_own, 'o-', 'Color', c, 'MarkerFaceColor', c, ...
             'MarkerSize', 3, 'LineWidth', 1.2, 'DisplayName', sprintf('v%d', p + 1));
    end
    style_axes(ax, K);
    shade_warmup(ax, K, WARMUP);
    xlabel(ax, 'Requests completed in the run');
    ylabel(ax, 'Mean learned threshold s^* for its own client');
    if col == 1
        lg = legend(ax, 'Location', 'southwest', 'FontSize', 7);
        title(lg, 'cache', 'FontWeight', 'normal');
    end
end
title(tl, 'Delay-aware caches over a run (grey = warm-up, excluded from the results)', ...
      'FontSize', 10);
exportgraphics(fig, fullfile(OUT, 'stage4_convergence.pdf'), 'ContentType', 'vector');
exportgraphics(fig, fullfile(OUT, 'stage4_convergence.png'), 'Resolution', 200);
end

function replay(D, N, K, OUT, x, runId, WARMUP)
% One run, one frame per 100 requests: its delay so far, and each cache's D_up and s* right now.
shown = series_styles(K, best_fixed(D, x, WARMUP));
one = N(near(N.offered_load, x) & N.run == runId, :);
steps = unique(one.request)';
[fig, tl] = new_figure(9, 6, 2, 2);

axD = nexttile(tl, 1, [1 2]);
hold(axD, 'on');
h = gobjects(1, numel(shown));
ymax = 0;
for i = 1:numel(shown)
    R = sortrows(D(near(D.offered_load, x) & D.policy == shown(i).policy & D.run == runId, :), ...
                 'request');
    shown(i).x = R.request;
    shown(i).y = R.mean_ms;
    ymax = max(ymax, max(R.mean_ms));
    h(i) = plot(axD, NaN, NaN, [shown(i).marker shown(i).line], 'Color', shown(i).color, ...
                'MarkerFaceColor', shown(i).color, 'MarkerSize', 4, ...
                'LineWidth', shown(i).width, 'DisplayName', shown(i).name);
end
style_axes(axD, K);
xlim(axD, [0 max(steps)]);
ylim(axD, [0 1.05 * ymax]);
shade_warmup(axD, K, WARMUP);
xlabel(axD, 'Requests completed in the run');
ylabel(axD, 'Mean delay per 100 requests (ms)');
legend(axD, 'Location', 'northeast', 'FontSize', 7);

axU = nexttile(tl, 3);
bU = bar(axU, 1:5, zeros(1, 5), 'FaceColor', 'flat', 'EdgeColor', 'none');
bU.CData = K.positionRamp;
style_axes(axU, K);
axU.XGrid = 'off';
ylim(axU, [0 1.05 * max(one.upstream_ms)]);
xticklabels(axU, compose('v%d', 1:5));
ylabel(axU, 'Upstream delay D_{up} now (ms)');

axS = nexttile(tl, 4);
bS = bar(axS, 1:5, zeros(1, 5), 'FaceColor', 'flat', 'EdgeColor', 'none');
bS.CData = K.positionRamp;
style_axes(axS, K);
axS.XGrid = 'off';
ylim(axS, [0 1.05]);
xticklabels(axS, compose('v%d', 1:5));
ylabel(axS, 'Learned threshold s^* now');

file = fullfile(OUT, 'stage4_replay.gif');
for k = 1:numel(steps)
    for i = 1:numel(shown)
        upto = shown(i).x <= steps(k);
        set(h(i), 'XData', shown(i).x(upto), 'YData', shown(i).y(upto));
    end
    snap = sortrows(one(one.request == steps(k), :), 'position');
    bU.YData = snap.upstream_ms';
    bS.YData = snap.threshold_own';
    phase = 'measured';
    if steps(k) <= WARMUP
        phase = 'warm-up';
    end
    title(tl, sprintf('Load %g, run %d: request %d of %d (%s)', x, runId, steps(k), ...
                      max(steps), phase), 'FontSize', 10);
    drawnow;
    [A, map] = rgb2ind(frame2im(getframe(fig)), 256);
    if k == 1
        imwrite(A, map, file, 'gif', 'LoopCount', Inf, 'DelayTime', 0.6);
    else
        imwrite(A, map, file, 'gif', 'WriteMode', 'append', 'DelayTime', 0.6);
    end
end
end


%% ---------------------------------------------------------------- helpers

function shown = series_styles(K, best)
% The three policies drawn in every panel: ours, the oracle, and the best fixed alpha at this load.
shown = [K.delay_aware, K.threshold, K.exclusion];
shown(3).policy = best;
shown(3).name = sprintf('best fixed \\alpha: exclusion \\alpha = %s', extractAfter(best, '-a'));
end

function best = best_fixed(D, x, WARMUP)
% The fixed-alpha exclusion policy with the lowest mean delay after warm-up at load x.
R = D(near(D.offered_load, x) & startsWith(D.policy, "exclusion-a") & D.request > WARMUP, :);
G = groupsummary(R, 'policy', 'mean', 'mean_ms');
[~, i] = min(G.mean_mean_ms);
best = char(G.policy(i));
end

function t = t975(df)
% Student-t 97.5% quantile for the run counts used here (base MATLAB has no tinv).
q = [12.706 4.303 3.182 2.776 2.571 2.447 2.365 2.306 2.262];
t = q(df)';
end

function shade_warmup(ax, K, WARMUP)
yl = ylim(ax);
h = patch(ax, [0 WARMUP WARMUP 0], [yl(1) yl(1) yl(2) yl(2)], K.grid, 'EdgeColor', 'none', ...
          'FaceAlpha', 0.6, 'HandleVisibility', 'off');
uistack(h, 'bottom');
ylim(ax, yl);
end

function K = palette()
% Same colours as report_figures.m: one fixed hue per policy, a blue ramp for cache position.
rgb = @(hex) reshape(sscanf(hex(2:end), '%2x'), 1, 3) / 255;
ramp = @(hexes) cell2mat(cellfun(rgb, hexes', 'UniformOutput', false));
K.ink2 = rgb('#52514e');
K.grid = rgb('#e1e0d9');
K.delay_aware = struct('policy', 'delay-aware', 'name', 'delay-aware (ours)', ...
                       'color', rgb('#2a78d6'), 'marker', 'o', 'line', '-', 'width', 2.2, ...
                       'x', [], 'y', []);
K.threshold = struct('policy', 'threshold-a1', 'name', 'oracle: s_{min} in Interest', ...
                     'color', rgb('#1baf7a'), 'marker', 's', 'line', '--', 'width', 1.5, ...
                     'x', [], 'y', []);
K.exclusion = struct('policy', '', 'name', '', ...
                     'color', rgb('#4a3aa7'), 'marker', 'd', 'line', '-', 'width', 1.5, ...
                     'x', [], 'y', []);
K.positionRamp = ramp({'#86b6ef' '#5598e7' '#2a78d6' '#1c5cab' '#104281'});
end

function [fig, tl] = new_figure(width, height, rows, cols)
fig = figure('Color', 'w', 'WindowStyle', 'normal', 'Units', 'inches', ...
             'Position', [0.5 0.5 width height]);
tl = tiledlayout(fig, rows, cols, 'TileSpacing', 'compact', 'Padding', 'compact');
end

function style_axes(ax, K)
ax.FontSize = 9;
ax.Box = 'off';
ax.TickDir = 'out';
ax.XColor = K.ink2;
ax.YColor = K.ink2;
ax.GridColor = K.grid;
ax.GridAlpha = 1;
ax.XGrid = 'on';
ax.YGrid = 'on';
ax.Layer = 'bottom';
end

function tf = near(x, v)
tf = abs(x - v) < 1e-9;
end

function T = load_csv(name)
T = readtable(name, 'TextType', 'string', 'Delimiter', ',');
end
