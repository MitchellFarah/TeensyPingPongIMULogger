function log = decodeImuLog(sessionPathOrStem, Ntrim)
% decodeImuLog  Decode Teensy IMU logger session (.json + .bin) using chunk-aware parsing
%
%   log = decodeImuLog(sessionPathOrStem)
%   log = decodeImuLog(sessionPathOrStem, Ntrim)
%
% INPUT:
%   sessionPathOrStem: stem "imuLog_00" OR a path to .json/.bin
%   Ntrim (optional): number of initial samples to discard (default 0)
%
% OUTPUT:
%   log.meta, log.files, log.raw, log.phys, log.time, log.timetable
%
% Assumes record layout (28 bytes, little-endian):
%   uint32 seq; uint32 ts_us; int16 ax,ay,az,gx,gy,gz,temp,mx,my,mz

    if nargin < 1 || (isstring(sessionPathOrStem) && strlength(sessionPathOrStem)==0)
        error('Provide a session stem (e.g., "imuLog_00") or a path to .json/.bin.');
    end
    if nargin < 2 || isempty(Ntrim), Ntrim = 0; end
    if ~isscalar(Ntrim) || Ntrim < 0 || fix(Ntrim) ~= Ntrim
        error('Ntrim must be a nonnegative integer scalar.');
    end

    sessionPathOrStem = char(sessionPathOrStem);

    % ---- Resolve filenames ----
    [folder, name, ext] = fileparts(sessionPathOrStem);
    if isempty(folder), folder = pwd; end

    if strcmpi(ext,'.json') || strcmpi(ext,'.bin')
        stem = name;
    else
        stem = sessionPathOrStem;
    end

    jsonFile = fullfile(folder, [stem '.json']);
    binFile  = fullfile(folder, [stem '.bin']);

    if ~isfile(jsonFile), error('JSON file not found: %s', jsonFile); end
    if ~isfile(binFile),  error('BIN file not found: %s', binFile);  end

    % ---- Load metadata ----
    meta = jsonToStruct(jsonFile);

    if ~isfield(meta,'buffer_config') || ~isfield(meta.buffer_config,'chunk_bytes')
        error('JSON missing buffer_config.chunk_bytes');
    end
    if ~isfield(meta,'file_metadata') || ~isfield(meta.file_metadata,'sample_count')
        error('JSON missing file_metadata.sample_count');
    end

    chunkBytes  = double(meta.buffer_config.chunk_bytes);
    sampleCount = double(meta.file_metadata.sample_count);

    recBytes = 28;
    nRecPerChunk = floor(chunkBytes / recBytes);
    padBytes     = mod(chunkBytes, recBytes); 

    if sampleCount <= 0
        error('Invalid sample_count in JSON.');
    end

    % ---- Preallocate output arrays ----
    seq   = zeros(sampleCount,1,'uint32');
    ts_us = zeros(sampleCount,1,'uint32');

    ax = zeros(sampleCount,1,'int16'); ay = ax; az = ax;
    gx = zeros(sampleCount,1,'int16'); gy = gx; gz = gx;
    temp = zeros(sampleCount,1,'int16');
    mx = zeros(sampleCount,1,'int16'); my = mx; mz = mx;

    % ---- Read file chunk-aware (little-endian) ----
    fid = fopen(binFile,'rb','ieee-le');
    if fid == -1, error('Could not open BIN file: %s', binFile); end
    cleaner = onCleanup(@() fclose(fid));

    k = 0; % samples written so far

    while k < sampleCount
        raw = fread(fid, chunkBytes, 'uint8=>uint8');
        if isempty(raw)
            break
        end

        % decode only whole records from whatever we got
        nRec = floor(numel(raw) / recBytes);
        if nRec == 0
            break
        end

        raw = raw(1:nRec*recBytes);
        raw = reshape(raw, recBytes, nRec);

        % decode all fields (typecast assumes little-endian ordering matches file,
        % which is true because we opened with ieee-le)
        s  = typecast(reshape(raw(1:4,:),  [],1), 'uint32');
        t  = typecast(reshape(raw(5:8,:),  [],1), 'uint32');

        axb = typecast(reshape(raw(9:10,:),  [],1), 'int16');
        ayb = typecast(reshape(raw(11:12,:), [],1), 'int16');
        azb = typecast(reshape(raw(13:14,:), [],1), 'int16');

        gxb = typecast(reshape(raw(15:16,:), [],1), 'int16');
        gyb = typecast(reshape(raw(17:18,:), [],1), 'int16');
        gzb = typecast(reshape(raw(19:20,:), [],1), 'int16');

        tb  = typecast(reshape(raw(21:22,:), [],1), 'int16');

        mxb = typecast(reshape(raw(23:24,:), [],1), 'int16');
        myb = typecast(reshape(raw(25:26,:), [],1), 'int16');
        mzb = typecast(reshape(raw(27:28,:), [],1), 'int16');

        % write into preallocated arrays
        nTake = min(nRec, sampleCount - k);

        idx = (k+1):(k+nTake);

        seq(idx)   = s(1:nTake);
        ts_us(idx) = t(1:nTake);

        ax(idx) = axb(1:nTake); ay(idx) = ayb(1:nTake); az(idx) = azb(1:nTake);
        gx(idx) = gxb(1:nTake); gy(idx) = gyb(1:nTake); gz(idx) = gzb(1:nTake);
        temp(idx) = tb(1:nTake);
        mx(idx) = mxb(1:nTake); my(idx) = myb(1:nTake); mz(idx) = mzb(1:nTake);

        k = k + nTake;
    end

    if k < sampleCount
        warning('File ended early: expected %d samples, decoded %d.', sampleCount, k);
        seq   = seq(1:k);
        ts_us = ts_us(1:k);
        ax = ax(1:k); ay = ay(1:k); az = az(1:k);
        gx = gx(1:k); gy = gy(1:k); gz = gz(1:k);
        temp = temp(1:k);
        mx = mx(1:k); my = my(1:k); mz = mz(1:k);
        sampleCount = k;
    end

    % ---- Apply optional initial trim ----
    if Ntrim > 0
        if sampleCount <= Ntrim
            error('Ntrim=%d but log only has %d samples.', Ntrim, sampleCount);
        end
        idx = (Ntrim+1):sampleCount;

        seq   = seq(idx);
        ts_us = ts_us(idx);
        ax = ax(idx); ay = ay(idx); az = az(idx);
        gx = gx(idx); gy = gy(idx); gz = gz(idx);
        temp = temp(idx);
        mx = mx(idx); my = my(idx); mz = mz(idx);
        sampleCount = numel(seq);
    end

    % ---- Convert to physical units ----
    accelLSBperG  = accelScaleFromRange(meta);
    gyroLSBperDPS = gyroScaleFromRange(meta);

    accel_g  = double([ax, ay, az]) ./ accelLSBperG;
    gyro_dps = double([gx, gy, gz]) ./ gyroLSBperDPS;

    % Magnetometer conversion: 0.15 µT per LSB
    mag_uT = double([mx, my, mz]) * 0.15;

    % Temperature conversion (same as before; adjust if your IMU differs)
    temp_C = (double(temp) - 21) ./ 333.87 + 21;

    % ---- Time vectors ----
    t_us = double(ts_us);
    t_s  = (t_us - t_us(1)) / 1e6;

    % ---- Pack output ----
    log = struct();
    log.meta  = meta;
    log.files = struct('json', jsonFile, 'bin', binFile);

    log.raw  = struct('seq',seq,'ts_us',ts_us,'ax',ax,'ay',ay,'az',az,'gx',gx,'gy',gy,'gz',gz,'temp',temp,'mx',mx,'my',my,'mz',mz);
    log.phys = struct('accel_g',accel_g,'gyro_dps',gyro_dps,'mag_uT',mag_uT,'temp_C',temp_C);
    log.time = struct('t_us',t_us,'t_s',t_s);

    try
        log.timetable = timetable(seconds(t_s), accel_g(:,1), accel_g(:,2), accel_g(:,3), ...
                                  gyro_dps(:,1), gyro_dps(:,2), gyro_dps(:,3), ...
                                  mag_uT(:,1), mag_uT(:,2), mag_uT(:,3), temp_C, ...
            'VariableNames', {'ax_g','ay_g','az_g','gx_dps','gy_dps','gz_dps','mx_uT','my_uT','mz_uT','temp_C'});
    catch
        log.timetable = [];
    end
end

function lsbPerG = accelScaleFromRange(meta)
    lsbPerG = 16384.0; % ±2g default
    if isfield(meta,'imu_config') && isfield(meta.imu_config,'accel') && isfield(meta.imu_config.accel,'full_scale_range')
        fs = string(meta.imu_config.accel.full_scale_range);
        if contains(fs,"2"), lsbPerG = 16384.0;
        elseif contains(fs,"4"), lsbPerG = 8192.0;
        elseif contains(fs,"8"), lsbPerG = 4096.0;
        elseif contains(fs,"16"), lsbPerG = 2048.0;
        end
    end
end

function lsbPerDPS = gyroScaleFromRange(meta)
    lsbPerDPS = 131.07; % ±250 dps default
    if isfield(meta,'imu_config') && isfield(meta.imu_config,'gyro') && isfield(meta.imu_config.gyro,'full_scale_range')
        fs = string(meta.imu_config.gyro.full_scale_range);
        if contains(fs,"250"), lsbPerDPS = 131.07;
        elseif contains(fs,"500"), lsbPerDPS = 65.535;
        elseif contains(fs,"1000"), lsbPerDPS = 32.768;
        elseif contains(fs,"2000"), lsbPerDPS = 16.384;
        end
    end
end



%%%% PREVIOUS VERSION(s) %%%%%
% Newest:
% function log = decodeImuLog(sessionPathOrStem, Ntrim)
% % decodeImuLog  Decode Teensy 4.1 IMU logger session (.json + .bin) to struct
% %
% %   log = decodeImuLog(sessionPathOrStem)
% %   log = decodeImuLog(sessionPathOrStem, Ntrim)
% %
% % INPUT:
% %   sessionPathOrStem:
% %     - stem: "imuLog_00"  -> loads imuLog_00.json and imuLog_00.bin
% %     - path to .json or .bin: "binFiles\imuLog_00.json"
% %   Ntrim (optional):
% %     - number of initial samples to remove from ALL outputs (default 0)
% %
% % OUTPUT:
% %   log.meta      : JSON metadata struct (from jsonToStruct)
% %   log.files     : resolved file paths
% %   log.raw       : raw integer vectors (seq, ts_us, ax..temp) [trimmed]
% %   log.phys      : physical units (accel_g, gyro_dps, temp_C) [trimmed]
% %   log.time      : time vectors (t_us, t_s) [trimmed, re-zeroed]
% %   log.timetable : timetable (if available) [trimmed]
% %
% % Binary record (22 bytes, little-endian):
% %   uint32 seq; uint32 ts_us; int16 ax,ay,az,gx,gy,gz,temp
% % See IMU_LOGGER_FORMAT.md for full spec. :contentReference[oaicite:0]{index=0}
% 
%     if nargin < 1 || (isstring(sessionPathOrStem) && strlength(sessionPathOrStem)==0)
%         error('Provide a session stem (e.g., "imuLog_00") or a path to .json/.bin.');
%     end
%     if nargin < 2 || isempty(Ntrim)
%         Ntrim = 0;
%     end
%     if ~isscalar(Ntrim) || Ntrim < 0 || fix(Ntrim) ~= Ntrim
%         error('Ntrim must be a nonnegative integer scalar.');
%     end
% 
%     sessionPathOrStem = char(sessionPathOrStem);
% 
%     % ---------- Resolve filenames ----------
%     [folder, name, ext] = fileparts(sessionPathOrStem);
%     if isempty(folder), folder = pwd; end
% 
%     if strcmpi(ext,'.json') || strcmpi(ext,'.bin')
%         stem = name;
%     else
%         stem = sessionPathOrStem;
%     end
% 
%     jsonFile = fullfile(folder, [stem '.json']);
%     binFile  = fullfile(folder, [stem '.bin']);
% 
%     if ~isfile(jsonFile), error('JSON file not found: %s', jsonFile); end
%     if ~isfile(binFile),  error('BIN file not found: %s', binFile);  end
% 
%     % ---------- Load metadata ----------
%     meta = jsonToStruct(jsonFile);
% 
%     % Record size (bytes). Spec says 22 bytes; JSON also stores it. :contentReference[oaicite:1]{index=1} :contentReference[oaicite:2]{index=2}
%     recSize = 22;
%     if isfield(meta,'buffer_config') && isfield(meta.buffer_config,'record_size_bytes')
%         recSize = double(meta.buffer_config.record_size_bytes);
%     end
% 
%     % ---------- Open BIN (little-endian) ----------
%     fid = fopen(binFile, 'rb', 'ieee-le');   % Teensy 4.1 is little-endian :contentReference[oaicite:3]{index=3}
%     if fid == -1
%         error('Could not open BIN file: %s', binFile);
%     end
%     cleaner = onCleanup(@() fclose(fid));
% 
%     % ---------- Determine number of records robustly ----------
%     nRec = [];
% 
%     % Prefer JSON sample_count if present (most reliable for trimming prealloc) :contentReference[oaicite:4]{index=4}
%     if isfield(meta,'file_metadata') && isfield(meta.file_metadata,'sample_count')
%         sc = double(meta.file_metadata.sample_count);
%         if ~isnan(sc) && sc > 0
%             nRec = sc;
%         end
%     end
% 
%     % Fall back to filesize if sample_count missing
%     if isempty(nRec)
%         fseek(fid, 0, 'eof');
%         fileBytes = ftell(fid);
%         fseek(fid, 0, 'bof');
% 
%         nRec = floor(double(fileBytes) / recSize);
%         leftover = double(fileBytes) - nRec * recSize;
%         if leftover ~= 0
%             warning('BIN size not multiple of record size (%d). Ignoring %d trailing bytes.', recSize, leftover);
%         end
%         if nRec == 0
%             error('BIN file is too small to contain any records.');
%         end
%     end
% 
%     % ---------- Read fields (fread with skip) ----------
%     fseek(fid, 0, 'bof');     seq   = fread(fid, nRec, 'uint32=>uint32', recSize-4, 'ieee-le');
%     fseek(fid, 4, 'bof');     ts_us = fread(fid, nRec, 'uint32=>uint32', recSize-4, 'ieee-le');
% 
%     fseek(fid, 8,  'bof');    ax   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 10, 'bof');    ay   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 12, 'bof');    az   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 14, 'bof');    gx   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 16, 'bof');    gy   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 18, 'bof');    gz   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 20, 'bof');    temp = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
% 
%     % ---------- Safety trim if timestamps go backwards (prealloc garbage) ----------
%     dts = diff(double(ts_us));
%     badIdx = find(dts < 0, 1, 'first');   % first backwards jump
%     if ~isempty(badIdx)
%         keepN = badIdx; % keep up through last good sample
%         seq   = seq(1:keepN);
%         ts_us = ts_us(1:keepN);
%         ax = ax(1:keepN); ay = ay(1:keepN); az = az(1:keepN);
%         gx = gx(1:keepN); gy = gy(1:keepN); gz = gz(1:keepN);
%         temp = temp(1:keepN);
%         nRec = keepN;
%         warning('Detected non-monotonic timestamps; trimmed to %d samples.', keepN);
%     end
% 
%     % ---------- Apply user-requested initial trim (Ntrim) ----------
%     if Ntrim > 0
%         if nRec <= Ntrim
%             error('Ntrim=%d but log only has %d samples after trimming.', Ntrim, nRec);
%         end
% 
%         idx = (Ntrim+1):nRec;
% 
%         seq   = seq(idx);
%         ts_us = ts_us(idx);
% 
%         ax = ax(idx); ay = ay(idx); az = az(idx);
%         gx = gx(idx); gy = gy(idx); gz = gz(idx);
%         temp = temp(idx);
%     end
% 
%     % ---------- Convert to physical units ----------
%     accelLSBperG  = accelScaleFromRange(meta);   % defaults match example conversion :contentReference[oaicite:5]{index=5}
%     gyroLSBperDPS = gyroScaleFromRange(meta);    % defaults match example conversion :contentReference[oaicite:6]{index=6}
% 
%     accel_g  = double([ax, ay, az]) ./ accelLSBperG;
%     gyro_dps = double([gx, gy, gz]) ./ gyroLSBperDPS;
% 
%     % Temperature conversion from documentation example :contentReference[oaicite:7]{index=7}
%     temp_C = (double(temp) - 21) ./ 333.87 + 21;
% 
%     % ---------- Time vectors (re-zero after Ntrim) ----------
%     t_us = double(ts_us);
%     t_s  = (t_us - t_us(1)) / 1e6;
% 
%     % ---------- Pack output ----------
%     log = struct();
%     log.meta = meta;
%     log.files = struct('json', jsonFile, 'bin', binFile);
% 
%     log.raw = struct('seq',seq,'ts_us',ts_us,'ax',ax,'ay',ay,'az',az,'gx',gx,'gy',gy,'gz',gz,'temp',temp);
%     log.phys = struct('accel_g',accel_g,'gyro_dps',gyro_dps,'temp_C',temp_C);
%     log.time = struct('t_us',t_us,'t_s',t_s);
% 
%     % ---------- Timetable (also trimmed since it uses trimmed arrays) ----------
%     try
%         log.timetable = timetable(seconds(t_s), accel_g(:,1), accel_g(:,2), accel_g(:,3), ...
%                                   gyro_dps(:,1), gyro_dps(:,2), gyro_dps(:,3), temp_C, ...
%             'VariableNames', {'ax_g','ay_g','az_g','gx_dps','gy_dps','gz_dps','temp_C'});
%     catch
%         log.timetable = [];
%     end
% end
% 
% % ---- local helpers ----
% 
% function lsbPerG = accelScaleFromRange(meta)
%     % Default consistent with ±2g example: 16384 LSB/g :contentReference[oaicite:8]{index=8}
%     lsbPerG = 16384.0;
% 
%     if isfield(meta,'imu_config') && isfield(meta.imu_config,'accel') && isfield(meta.imu_config.accel,'full_scale_range')
%         fs = string(meta.imu_config.accel.full_scale_range);
%         if contains(fs,"2"),       lsbPerG = 16384.0;
%         elseif contains(fs,"4"),   lsbPerG = 8192.0;
%         elseif contains(fs,"8"),   lsbPerG = 4096.0;
%         elseif contains(fs,"16"),  lsbPerG = 2048.0;
%         end
%     end
% end
% 
% function lsbPerDPS = gyroScaleFromRange(meta)
%     % Default consistent with ±250 dps example: 131.07 LSB/(deg/s) :contentReference[oaicite:9]{index=9}
%     lsbPerDPS = 131.07;
% 
%     if isfield(meta,'imu_config') && isfield(meta.imu_config,'gyro') && isfield(meta.imu_config.gyro,'full_scale_range')
%         fs = string(meta.imu_config.gyro.full_scale_range);
%         if contains(fs,"250"),        lsbPerDPS = 131.07;
%         elseif contains(fs,"500"),    lsbPerDPS = 65.535;
%         elseif contains(fs,"1000"),   lsbPerDPS = 32.768;
%         elseif contains(fs,"2000"),   lsbPerDPS = 16.384;
%         end
%     end
% end


% Oldedst:
% function log = decodeImuLog(sessionPathOrStem)
% % decodeImuLog  Decode Teensy 4.1 IMU logger session (.json + .bin) to struct
% %
% %   log = decodeImuLog(sessionPathOrStem)
% %
% % INPUT:
% %   sessionPathOrStem can be:
% %     - stem: "imuLog_00"  -> loads imuLog_00.json and imuLog_00.bin
% %     - path to .json or .bin: "binFiles\imuLog_00.json"
% %
% % OUTPUT:
% %   log.meta      : JSON metadata struct
% %   log.files     : resolved file paths
% %   log.raw       : raw integer vectors (seq, ts_us, ax..temp)
% %   log.phys      : physical units (accel_g, gyro_dps, temp_C)
% %   log.time      : time vectors (t_us, t_s)
% %   log.timetable : timetable (if available)
% %
% % Binary record (22 bytes, little-endian):
% %   uint32 seq; uint32 ts_us; int16 ax,ay,az,gx,gy,gz,temp
% % See IMU_LOGGER_FORMAT.md for full spec. 
% 
%     if nargin < 1 || (isstring(sessionPathOrStem) && strlength(sessionPathOrStem)==0)
%         error('Provide a session stem (e.g., "imuLog_00") or a path to .json/.bin.');
%     end
%     sessionPathOrStem = char(sessionPathOrStem);
% 
%     % ---------- Resolve filenames ----------
%     [folder, name, ext] = fileparts(sessionPathOrStem);
%     if isempty(folder), folder = pwd; end
% 
%     if strcmpi(ext,'.json') || strcmpi(ext,'.bin')
%         stem = name;
%     else
%         stem = sessionPathOrStem;
%     end
% 
%     jsonFile = fullfile(folder, [stem '.json']);
%     binFile  = fullfile(folder, [stem '.bin']);
% 
%     if ~isfile(jsonFile)
%         error('JSON file not found: %s', jsonFile);
%     end
%     if ~isfile(binFile)
%         error('BIN file not found: %s', binFile);
%     end
% 
%     % ---------- Load metadata (your helper) ----------
%     meta = jsonToStruct(jsonFile);
% 
%     % Record size (bytes). Spec says 22 bytes; JSON also stores it. 
%     recSize = 22;
%     if isfield(meta,'buffer_config') && isfield(meta.buffer_config,'record_size_bytes')
%         recSize = double(meta.buffer_config.record_size_bytes);
%     end
%     if recSize ~= 22
%         warning('Record size in JSON is %g bytes (expected 22). Proceeding with %g.', recSize, recSize);
%     end
% 
%     % ---------- Open and size-check BIN ----------
%     fid = fopen(binFile, 'rb', 'ieee-le');  % Teensy 4.1 is little-endian 
%     if fid == -1
%         error('Could not open BIN file: %s', binFile);
%     end
%     cleaner = onCleanup(@() fclose(fid));
% 
%     fseek(fid, 0, 'eof');
%     fileBytes = ftell(fid);
%     fseek(fid, 0, 'bof');
% 
%     nRec = floor(double(fileBytes) / recSize);
%     leftover = double(fileBytes) - nRec * recSize;
%     if nRec == 0
%         error('BIN file is too small to contain any records.');
%     end
%     if leftover ~= 0
%         warning('BIN size is not a multiple of record size (%d). Ignoring %d trailing bytes.', recSize, leftover);
%     end
% 
%     % ---------- Read fields (best-practice: fread with precision + skip) ----------
%     %
%     % Record layout: [seq uint32][ts uint32][7x int16]
%     % Total = 4 + 4 + 14 = 22 bytes
%     %
%     % For a field, we:
%     %   - fseek to its byte offset within first record
%     %   - fread with "skip" = recSize - bytes_of_that_field
%     %
%     % NOTE: All reads use 'ieee-le' so values are correctly interpreted.
%     %
% 
%     % uint32 fields
%     fseek(fid, 0, 'bof');     seq   = fread(fid, nRec, 'uint32=>uint32', recSize-4, 'ieee-le');
%     fseek(fid, 4, 'bof');     ts_us = fread(fid, nRec, 'uint32=>uint32', recSize-4, 'ieee-le');
% 
%     % int16 fields (offsets in bytes from start of record)
%     fseek(fid, 8,  'bof');    ax   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 10, 'bof');    ay   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 12, 'bof');    az   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 14, 'bof');    gx   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 16, 'bof');    gy   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 18, 'bof');    gz   = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
%     fseek(fid, 20, 'bof');    temp = fread(fid, nRec, 'int16=>int16',  recSize-2, 'ieee-le');
% 
%     % ---------- Convert to physical units ----------
%     accelLSBperG  = accelScaleFromRange(meta);  % defaults match example conversion
%     gyroLSBperDPS = gyroScaleFromRange(meta);
% 
%     accel_g  = double([ax, ay, az]) ./ accelLSBperG;
%     gyro_dps = double([gx, gy, gz]) ./ gyroLSBperDPS;
% 
%     % Temperature conversion from documentation example 
%     temp_C = (double(temp) - 21) ./ 333.87 + 21;
% 
%     % ---------- Time vectors ----------
%     t_us = double(ts_us);
%     t_s  = (t_us - t_us(1)) / 1e6;
% 
%     % ---------- Pack output ----------
%     log = struct();
%     log.meta = meta;
%     log.files = struct('json', jsonFile, 'bin', binFile);
% 
%     log.raw = struct();
%     log.raw.seq   = seq;
%     log.raw.ts_us = ts_us;
%     log.raw.ax = ax; log.raw.ay = ay; log.raw.az = az;
%     log.raw.gx = gx; log.raw.gy = gy; log.raw.gz = gz;
%     log.raw.temp = temp;
% 
%     log.phys = struct();
%     log.phys.accel_g  = accel_g;
%     log.phys.gyro_dps = gyro_dps;
%     log.phys.temp_C   = temp_C;
% 
%     log.time = struct();
%     log.time.t_us = t_us;
%     log.time.t_s  = t_s;
% 
%     % Optional timetable (nice for plotting/analysis)
%     try
%         log.timetable = timetable(seconds(t_s), accel_g(:,1), accel_g(:,2), accel_g(:,3), ...
%                                   gyro_dps(:,1), gyro_dps(:,2), gyro_dps(:,3), temp_C, ...
%             'VariableNames', {'ax_g','ay_g','az_g','gx_dps','gy_dps','gz_dps','temp_C'});
%     catch
%         log.timetable = [];
%     end
% end
% 
% % ---- local helpers ----
% 
% function lsbPerG = accelScaleFromRange(meta)
%     % Default consistent with ±2g example: 16384 LSB/g 
%     lsbPerG = 16384.0;
% 
%     if isfield(meta,'imu_config') && isfield(meta.imu_config,'accel') && isfield(meta.imu_config.accel,'full_scale_range')
%         fs = string(meta.imu_config.accel.full_scale_range);
%         if contains(fs,"2"),       lsbPerG = 16384.0;
%         elseif contains(fs,"4"),   lsbPerG = 8192.0;
%         elseif contains(fs,"8"),   lsbPerG = 4096.0;
%         elseif contains(fs,"16"),  lsbPerG = 2048.0;
%         end
%     end
% end
% 
% function lsbPerDPS = gyroScaleFromRange(meta)
%     % Default consistent with ±250 dps example: 131.07 LSB/(deg/s)
%     lsbPerDPS = 131.07;
% 
%     if isfield(meta,'imu_config') && isfield(meta.imu_config,'gyro') && isfield(meta.imu_config.gyro,'full_scale_range')
%         fs = string(meta.imu_config.gyro.full_scale_range);
%         if contains(fs,"250"),        lsbPerDPS = 131.07;
%         elseif contains(fs,"500"),    lsbPerDPS = 65.535;
%         elseif contains(fs,"1000"),   lsbPerDPS = 32.768;
%         elseif contains(fs,"2000"),   lsbPerDPS = 16.384;
%         end
%     end
% end
