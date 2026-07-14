%% PROJECT V.E.S.P.A. - Optical Chain Specification (Rev 4.0)
% Author: Chief System Architect
% Context: Mechatronic Final Year Project
% Hardware: NUBM49 + G-2 + D63F100 Aspheric Lens

clear; clc; close all;

%% 1. SYSTEM PARAMETERS (D63F100 CONFIGURATION)
P_laser = 5.5;          % Watts (Optical Power)
f_aspheric = 100;       % mm (New Lens from D63F100)
D_aspheric = 63;        % mm (New Lens Diameter)
dist_G2_Virt = 15;      % mm (Virtual source offset behind G-2)
Target_Range = [550, 1100]; % mm
Default_Target = 800;   % mm

% Safety/Lethality Constants
%
% PROVENANCE (added July 2026 -- see docs/src/vespa.bib):
%   T_exposure     20 ms. Consistent with Keller et al. 2020 (Sci. Rep. 10:14795,
%                  doi:10.1038/s41598-020-71824-y), which identifies ~25 ms as the
%                  ideal pulse duration to minimise required laser power, with no
%                  effect of target flight behaviour below that.  [keller2020photonicfence]
%
%   Fluence_Lethal 50 J/cm^2. *** UNSOURCED ASSUMPTION -- DO NOT TRUST. ***
%                  No laser-lethality measurement exists for any hornet. The only
%                  published dose-response curves are for far lighter insects:
%                  Keller 2020 reports LD90 = 1.6 J/cm^2 at 445 nm on Anopheles,
%                  i.e. ~30x LOWER than the value used here. The figure below is an
%                  undocumented extrapolation to the mass of a V. velutina worker.
%                  It drives P_laser -> I_lethal -> Class 4 -> the 200 m ocular
%                  hazard: erring high is safe for efficacy, UNSAFE for people.
%                  To redo properly: lethal thermal limit ~45 C for V. velutina
%                  (Ruiz-Cristi et al. 2020, PLoS ONE 15(10):e0239742,
%                  doi:10.1371/journal.pone.0239742) + thorax mass/area (Sipos 2025),
%                  via the dose method of Keller et al. 2016 (doi:10.1038/srep20936).
%
%   Wavelength     Blue (~460 nm) is not an arbitrary preference: visible wavelengths
%                  need far less exposure than NIR (1.6 vs 12.9 J/cm^2). [keller2020]
%
T_exposure = 0.020;     % s      -- corroborated (Keller 2020: ~25 ms optimal)
Fluence_Lethal = 50;    % J/cm^2 -- ASSUMPTION, not a measurement. See above.
I_lethal = Fluence_Lethal / T_exposure; % 2500 W/cm^2
I_MPE_Eye = 0.0025;     % W/cm^2
I_DryLeaf = 250;        % W/cm^2

%% 2. CONTROL LAW & BEAM STATS CALCULATION
targets = linspace(Target_Range(1), Target_Range(2), 200);
lens_pos_G2 = zeros(size(targets)); 
error_margin = zeros(size(targets)); 
theta_half_G2 = deg2rad(15); % 15 deg divergence from G-2

% Lethal Radius Calculation
w_lethal = sqrt(P_laser / (pi * I_lethal)) * 10; % mm

for i = 1:length(targets)
    Z_t = targets(i);
    Z_tot = Z_t + dist_G2_Virt;
    
    % Lens Position (u)
    roots_u = roots([1, -Z_tot, f_aspheric*Z_tot]);
    u_val = min(roots_u);
    lens_pos_G2(i) = u_val - dist_G2_Virt;
    
    % Beam Stats at this distance
    v_dist = Z_t - lens_pos_G2(i);
    w_at_lens = u_val * tan(theta_half_G2);
    theta_focus = atan(w_at_lens / v_dist);
    
    % Error Margin Calc (Geometric Approximation)
    w_0 = 0.08; % Conservative min spot size (mm)
    if w_lethal > w_0
        z_delta = (w_lethal - w_0) / tan(theta_focus);
        error_margin(i) = 2 * z_delta; 
    else
        error_margin(i) = 0;
    end
end

% --- SPECIFIC STATS FOR REPORT (DEFAULT POSITION) ---
[~, idx_def] = min(abs(targets - Default_Target));
Pos_Default = lens_pos_G2(idx_def); 
Linear_Travel = max(lens_pos_G2) - min(lens_pos_G2);
Margin_Default = error_margin(idx_def);

% Calculate Beam Diameter Entering Aspheric Lens
u_default = Pos_Default + dist_G2_Virt;
Beam_Radius_Input = u_default * tan(theta_half_G2);
Beam_Diameter_Input = 2 * Beam_Radius_Input;

%% 3. POWER DENSITY SIMULATION (At Default Target)
z_sim = linspace(0, 2000, 2000); 
theta_focus_def = atan(Beam_Radius_Input / (Default_Target - Pos_Default));
Power_Density = zeros(size(z_sim));

for k = 1:length(z_sim)
    dist_from_focus = abs(z_sim(k) - Default_Target);
    w_z = 0.08 + dist_from_focus * tan(theta_focus_def); 
    Power_Density(k) = P_laser / (pi * (w_z/10)^2);
end

Peak_Intensity = max(Power_Density);

%% 4. VISUALIZATION

% --- FIGURE 1: CONTROL LAW ---
figure('Name', 'VESPA Fig 1: Control Law (D63F100)', 'Color', 'white');
plot(lens_pos_G2, targets, 'LineWidth', 2, 'Color', '#0072BD');
grid on;
title('Control Law: Target Distance vs Lens Position');
xlabel('Lens Position from G-2 (mm)');
ylabel('Focus Distance (mm)');
axis tight;

% --- FIGURE 2: ERROR MARGIN ---
figure('Name', 'VESPA Fig 2: Allowed Error Margin', 'Color', 'white');
plot(targets, error_margin, 'LineWidth', 2, 'Color', '#77AC30');
grid on;
title('Allowed Depth Error (Lethal Zone) vs Target');
xlabel('Target Distance (mm)');
ylabel('Lethal Depth of Field (mm)');
yline(Margin_Default, '--k', sprintf('Margin @ 800mm: %.1f mm', Margin_Default));

% --- FIGURE 3: POWER DENSITY ---
figure('Name', 'VESPA Fig 3: Power Density', 'Color', 'white');
semilogy(z_sim, Power_Density, 'r', 'LineWidth', 1.5); hold on;
yline(I_lethal, '--k', 'Lethal Threshold');
yline(I_DryLeaf, '--g', 'Fire Limit');
yline(I_MPE_Eye, '-.b', 'Eye Safety');

% Formatting
ax = gca; ax.XAxis.Exponent = 0; xticks(0:200:2000);
xlim([400 1800]); ylim([1e-4 1e6]);
grid on;
title('Power Density Profile (Log Scale)');
xlabel('Distance from G-2 (mm)');
ylabel('Power Density (W/cm^2)');
legend('Beam Profile', 'Lethal', 'Fire Safety', 'Eye Safety', 'Location', 'southwest');

%% 5. FINAL REPORT OUTPUT
fprintf('==================================================\n');
fprintf('       PROJECT V.E.S.P.A. - OPTICAL REPORT        \n');
fprintf('==================================================\n');
fprintf('1. HARDWARE SELECTION\n');
fprintf('   * Aspheric Lens           : D63F100 (Dia 63mm, f=100mm)\n');
fprintf('   * Actuator                : Precision 35mm Stepper\n');
fprintf('--------------------------------------------------\n');
fprintf('2. BEAM STATISTICS (At 800mm Default Focus)\n');
fprintf('   * Lens Position (from G2) : %.2f mm\n', Pos_Default);
fprintf('   * Beam Diameter (Input)   : %.2f mm\n', Beam_Diameter_Input);
fprintf('     (Fill Factor: %.1f%% of 63mm aperture)\n', (Beam_Diameter_Input/D_aspheric)*100);
fprintf('   * Peak Intensity          : %.2e W/cm^2\n', Peak_Intensity);
fprintf('--------------------------------------------------\n');
fprintf('3. MECHANICAL REQUIREMENTS\n');
fprintf('   * Total Linear Travel     : %.2f mm\n', Linear_Travel);
fprintf('   * Lethal Depth (DoF)      : %.2f mm\n', Margin_Default);
fprintf('==================================================\n');