function data = jsonToStruct(filename)
% jsonToStruct  Read a JSON file and convert it to a MATLAB struct
%
%   data = jsonToStruct(filename)
%
%   INPUT:
%       filename - path to JSON file (string or char)
%
%   OUTPUT:
%       data     - MATLAB struct containing JSON contents

    % ----- Input check -----
    if nargin < 1
        error('You must provide a JSON filename.');
    end

    if ~isfile(filename)
        error('File not found: %s', filename);
    end

    % ----- Read file -----
    fid = fopen(filename, 'r');
    if fid == -1
        error('Could not open file.');
    end

    raw = fread(fid, inf, '*char')';
    fclose(fid);

    % ----- Convert JSON → struct -----
    data = jsondecode(raw);

end