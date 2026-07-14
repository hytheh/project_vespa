% -------------------------------------------------------------------------
% PROJECT V.E.S.P.A. - SYSTEM ARCHITECTURE - VISION MODULE (v2.3)
% -------------------------------------------------------------------------
% Script: Camera Placement, Frustum Visualization & GSD Analysis
% Author: Chief System Architect (AI) & Lead Engineer
% Context: Mechatronic Engineering Final Year Project
% Fix Log: v2.3 (Restored Height 'h' to the Legend)
% -------------------------------------------------------------------------

clear; clc; close all;

%% 1. INPUT VARIABLES (USER CONFIGURATION)
% -------------------------------------------------------------------------
% Killbox (Volume to protect)
KB.l = 450;         % Length (y) [mm]
KB.h = 300;         % Height (z) [mm]
KB.off = 80;      % Offset Y [mm] (Distance from origin to start of killbox)

% Sensor: Arducam OV9281 (Portrait Orientation)
Sens.HRes = 800;    % Horizontal Res (Mapped to World X)
Sens.VRes = 1280;   % Vertical Res (Mapped to World Y)
Sens.pix  = 0.003;  % Pixel pitch [mm]
Sens.W    = Sens.VRes * Sens.pix; % 3.84mm (Long side)
Sens.H    = Sens.HRes * Sens.pix; % 2.40mm (Short side)

% System Constraints
Cam.C_off = 160;    % Camera Y position [mm]
Req.GSD   = 0.5;    % Target GSD limit [mm/px] (Reference only)

% Lens Options for Comparison Graph
FocalLengths = [3, 4,4.6, 5, 6, 8];

% Select the specific lens to visualize in the 3D plot
% (must be within the defined options)
Selected_Focal_Length = 4.6; 
%% 2. CALCULATIONS (Iterative Solver)
% -------------------------------------------------------------------------
KB.w = KB.l/(Sens.VRes/Sens.HRes);         % Width (x) [mm]
results = [];
selected_config = [];

% Prepare Output Table
fprintf('\n-----------------------------------------------------\n');
fprintf('PROJECT V.E.S.P.A. - LENS SELECTION MATRIX\n');
fprintf('\nKB specs : %.2f x %.2f x %.2f\n',KB.w,KB.l,KB.h)
fprintf('-----------------------------------------------------\n');
fprintf('| Focal | Height(hc)| Tilt(deg) | GSD_Max | GSD_Min |\n');
fprintf('|-------|-----------|-----------|---------|---------|\n');

for i = 1:length(FocalLengths)
    f = FocalLengths(i);
    
    % --- A. Find Optimal Height & Tilt ---
    found = false;
    for h_try = 400:5:3000 
        
        % 1. Aim Logic: Look at the center of the killbox volume
        target_y = KB.off + KB.l/2;
        target_z = KB.h/2;
        
        dy = target_y - Cam.C_off;
        dz = h_try - target_z;
        tilt = atan2(dy, dz); 
        
        if tilt > deg2rad(60) || tilt < 0, continue; end
        
        % 2. Check coverage of the top plane
        corners_check = [
            -KB.w/2, KB.off,        KB.h;
             KB.w/2, KB.off,        KB.h;
             KB.w/2, KB.off + KB.l, KB.h;
            -KB.w/2, KB.off + KB.l, KB.h
        ];
        
        in_view = true;
        for c = 1:4
            P_world = corners_check(c,:)';
            [u, v] = world2cam(P_world, [0; Cam.C_off; h_try], tilt, f, Sens);
            if abs(u) > (Sens.HRes/2 * 0.95) || abs(v) > (Sens.VRes/2 * 0.95)
                in_view = false;
                break;
            end
        end
        
        if in_view
            h_opt = h_try;
            theta_opt = tilt;
            found = true;
            break;
        end
    end
    
    if ~found
        fprintf('| %2d mm |   FAIL    |    --     |   --    |   --    |\n', f);
        continue;
    end

    % --- B. Calculate GSD Curve (Midplane z=h/2) ---
    y_range = 0:10:(KB.off + KB.l + 100);
    gsd_curve = zeros(size(y_range));
    
    gsd_min_val = 100;
    gsd_max_val = 0;
    
    for j = 1:length(y_range)
        P_test = [0; y_range(j); KB.h/2];
        dist = norm(P_test - [0; Cam.C_off; h_opt]);
        gsd_val = (dist / f) * Sens.pix;
        gsd_curve(j) = gsd_val;
        
        % Only record Min/Max stats if we are INSIDE the Killbox Y-range
        if y_range(j) >= KB.off && y_range(j) <= (KB.off + KB.l)
            if gsd_val > gsd_max_val, gsd_max_val = gsd_val; end
            if gsd_val < gsd_min_val, gsd_min_val = gsd_val; end
        end
    end
    
    % Store data
    res.f = f;
    res.h = h_opt;
    res.tilt = theta_opt;
    res.gsd_curve = gsd_curve;
    res.y_range = y_range;
    res.gsd_min = gsd_min_val;
    res.gsd_max = gsd_max_val;
    results = [results, res];
    
    if f == Selected_Focal_Length
        selected_config = res;
    end
    
    % Print Data Row
    fprintf('|  %d mm |  %4d mm  |  %4.1f deg |  %0.3f  |  %0.3f  |\n', ...
        f, h_opt, rad2deg(theta_opt), gsd_max_val, gsd_min_val);
end
fprintf('-----------------------------------------------------\n');

%% 3. VISUALIZATION
% -------------------------------------------------------------------------
%fig = figure('Name', 'VESPA Vision Architecture - Final v2.3', 'Color', 'w', 'Position', [50 50 1400 700]);

% === LEFT PLOT: 3D GEOMETRY ===
figure()%subplot(1, 2, 1);
hold on; grid on; axis equal;
xlabel('X [mm]'); ylabel('Y [mm]'); zlabel('Z [mm]');
title(sprintf('3D Configuration (Lens: %dmm)', Selected_Focal_Length));
view(3); 

patch([-500 500 500 -500], [-200 -200 1000 1000], [0 0 0 0], 'k', 'FaceAlpha', 0.05, 'EdgeColor', 'none');
draw_box(KB.w, KB.l, KB.h, KB.off, [1 0 0], 0.1, '');

if ~isempty(selected_config)
    SC = selected_config;
    CamPos = [0; Cam.C_off; SC.h];
    vis_f = SC.f; 
    
    plot3(CamPos(1), CamPos(2), CamPos(3), 'ko', 'MarkerFaceColor', 'b', 'MarkerSize', 8);
    plot3([0 0], [Cam.C_off Cam.C_off], [SC.h 0], 'k--');
    
    opt_len = SC.h / cos(SC.tilt);
    vy = sin(SC.tilt); vz = -cos(SC.tilt);
    quiver3(0, Cam.C_off, SC.h, 0, vy*opt_len, vz*opt_len, 'Color', 'b', 'LineWidth', 1.5, 'MaxHeadSize', 0);
    
    r_arc = SC.h * 0.25;
    t_arc = linspace(-pi/2, -pi/2 + SC.tilt, 30);
    y_arc = Cam.C_off + r_arc * cos(t_arc);
    z_arc = SC.h      + r_arc * sin(t_arc);
    plot3(zeros(size(y_arc)), y_arc, z_arc, 'r-', 'LineWidth', 2);
    text(0, Cam.C_off, SC.h - r_arc - 50, sprintf('\\theta = %.1f\\circ', rad2deg(SC.tilt)), ...
         'Color', 'r', 'HorizontalAlignment', 'center');

    corners_uv = [-Sens.HRes/2, -Sens.VRes/2; Sens.HRes/2, -Sens.VRes/2;
                  Sens.HRes/2,  Sens.VRes/2; -Sens.HRes/2,  Sens.VRes/2];
    frustum_ground_pts = zeros(4,3); frustum_mid_pts = zeros(4,3);
    
    for k = 1:4
        u_mm = corners_uv(k,1) * Sens.pix; v_mm = corners_uv(k,2) * Sens.pix;
        D = vis_f * [0; sin(SC.tilt); -cos(SC.tilt)] - v_mm * [0; cos(SC.tilt); sin(SC.tilt)] + u_mm * [1; 0; 0];
        t_ground = -CamPos(3) / D(3); frustum_ground_pts(k,:) = CamPos' + t_ground * D';
        t_mid = (KB.h/2 - CamPos(3)) / D(3); frustum_mid_pts(k,:) = CamPos' + t_mid * D';
    end
    
    for k = 1:4
        line([CamPos(1), frustum_ground_pts(k,1)], [CamPos(2), frustum_ground_pts(k,2)], [CamPos(3), frustum_ground_pts(k,3)], 'Color', 'b', 'LineStyle', ':');
    end
    patch(frustum_ground_pts(:,1), frustum_ground_pts(:,2), frustum_ground_pts(:,3), 'b', 'FaceAlpha', 0.0, 'EdgeColor', 'b', 'LineStyle', '--');
    patch(frustum_mid_pts(:,1), frustum_mid_pts(:,2), frustum_mid_pts(:,3), 'b', 'FaceAlpha', 0.3, 'EdgeColor', 'b', 'LineWidth', 1.5);
    text(frustum_mid_pts(1,1), frustum_mid_pts(1,2), frustum_mid_pts(1,3), '', 'Color', 'b');
end
xlim([-500 500]); ylim([-100 1000]); zlim([0 1200]);

% === RIGHT PLOT: GSD EVOLUTION ===
figure()%subplot(1, 2, 2);
hold on; grid on;
title('GSD Evolution along Y-Axis (Midplane)');
xlabel('Y Distance [mm]'); ylabel('GSD [mm/px]');

yline(Req.GSD, 'r--', 'GSD_{Target} (0.5)', 'LabelHorizontalAlignment', 'right','HandleVisibility','off');
xline(KB.off, 'k:', 'Killbox Start','HandleVisibility','off'); 
xline(KB.off+KB.l, 'k:', 'Killbox End','HandleVisibility','off');

colors = lines(length(results));
for i = 1:length(results)
    lw = 1;
    if results(i).f == Selected_Focal_Length, lw = 2.5; end
    
    % *** RESTORED HEIGHT INDICATION ***
    lbl = sprintf('f=%dmm (h=%dmm | %.2f-%.2f)', results(i).f, round(results(i).h), results(i).gsd_min, results(i).gsd_max);
    
    plot(results(i).y_range, results(i).gsd_curve, 'Color', colors(i,:), 'LineWidth', lw, 'DisplayName', lbl);
end
legend('Location', 'northwest');
ylim([0.3 0.8]);

%% HELPER FUNCTIONS
function draw_box(w, l, h, y_off, col, alpha, lbl)
    V = [ -w/2, y_off, 0; w/2, y_off, 0; w/2, y_off+l, 0; -w/2, y_off+l, 0;
          -w/2, y_off, h; w/2, y_off, h; w/2, y_off+l, h; -w/2, y_off+l, h ];
    F = [1 2 3 4; 5 6 7 8; 1 2 6 5; 2 3 7 6; 3 4 8 7; 4 1 5 8];
    patch('Vertices', V, 'Faces', F, 'FaceColor', col, 'FaceAlpha', alpha);
    text(-w/2, y_off, h+20, lbl, 'Color', 'r');
end

function [u, v] = world2cam(P_world, Cam_Pos, theta, f, Sens)
    d = P_world - Cam_Pos;
    v_fwd = [0; sin(theta); -cos(theta)];
    v_up  = [0; cos(theta); sin(theta)];
    v_rt  = [1; 0; 0];
    z_c = dot(d, v_fwd); y_c = dot(d, -v_up); x_c = dot(d, v_rt);
    u = (x_c / z_c) * (f / Sens.pix);
    v = (y_c / z_c) * (f / Sens.pix);
end