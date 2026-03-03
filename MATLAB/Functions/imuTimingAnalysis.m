 function stats = imuTimingAnalysis(log)
% imuTimingAnalysis  Analyze sampling timing from IMU log
%
% INPUT:
%   log  -> output from decodeImuLog()
%
% OUTPUT:
%   stats struct with timing metrics

%% --- Extract timestamps ---
t_us = double(log.raw.ts_us);

% Convert to seconds
t_s = (t_us - t_us(1))/1e6;

%% --- Sample intervals ---
dt = diff(t_s);          % seconds between samples
fs = 1 ./ dt;            % instantaneous sampling rate

%% --- Core statistics ---
stats.mean_dt = mean(dt);
stats.std_dt  = std(dt);
stats.min_dt  = min(dt);
stats.max_dt  = max(dt);

stats.mean_fs = mean(fs);
stats.std_fs  = std(fs);

%% --- Jitter analysis ---
ideal_dt = stats.mean_dt;
jitter = dt - ideal_dt;

stats.jitter_rms = rms(jitter);
stats.jitter_peak = max(abs(jitter));

%% --- Dropped sample detection ---
% sequence counter should increment by 1
seq = double(log.raw.seq);
dseq = diff(seq);

dropIdx = find(dseq > 1);

stats.num_drops = sum(dseq(dseq>1)-1);
stats.drop_locations = dropIdx;

%% --- Print summary ---
fprintf('\n===== IMU Timing Analysis =====\n');

fprintf('Mean sample period : %.6f s\n', stats.mean_dt);
fprintf('Mean sampling rate : %.3f Hz\n', stats.mean_fs);

fprintf('Std period         : %.6e s\n', stats.std_dt);
fprintf('RMS jitter         : %.6e s\n', stats.jitter_rms);
fprintf('Peak jitter        : %.6e s\n', stats.jitter_peak);

fprintf('Min dt             : %.6f s\n', stats.min_dt);
fprintf('Max dt             : %.6f s\n', stats.max_dt);

fprintf('Dropped samples    : %d\n', stats.num_drops);

%% --- Visualization ---
figure('Name','IMU Timing Analysis');

subplot(3,1,1)
plot(t_s(2:end), dt*1000)
ylabel('dt (ms)')
title('Sample Period')
grid on

subplot(3,1,2)
histogram(dt*1000,100)
xlabel('dt (ms)')
ylabel('Count')
title('Sample Period Histogram')
grid on

subplot(3,1,3)
plot(t_s(2:end), jitter*1e6)
ylabel('Jitter (µs)')
xlabel('Time (s)')
title('Timing Jitter')
grid on

end
