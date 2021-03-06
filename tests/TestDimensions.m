function funcs = TestDimensions
%TestDimensions tests for Dimensions
%   Detailed explanation goes here

    funcs = {};
    funcs{end+1} = @test_set_dimension;
    funcs{end+1} = @test_sample_dimension;
    funcs{end+1} = @test_range_dimension;
end

function [] = test_set_dimension( varargin )
%% Test: set dimension
    f = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    b = f.createBlock('daTestBlock', 'test nixBlock');
    da = b.create_data_array('daTest', 'test nixDataArray', 'double', [1 2]);
    d1 = da.append_set_dimension();

    assert(strcmp(d1.dimensionType, 'set'));
    assert(isempty(d1.labels));
    
    d1.labels = {'foo', 'bar'};
    assert(strcmp(d1.labels{1}, 'foo'));
    assert(strcmp(d1.labels{2}, 'bar'));
    
    % fix this in NIX
    %d1.labels = {};
    %assert(isempty(d1.labels));
end

function [] = test_sample_dimension( varargin )
%% Test: sampled dimension
    f = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    b = f.createBlock('daTestBlock', 'test nixBlock');
    da = b.create_data_array('daTest', 'test nixDataArray', 'double', [1 2]);
    d1 = da.append_sampled_dimension(200);

    assert(strcmp(d1.dimensionType, 'sample'));
    assert(isempty(d1.label));
    assert(isempty(d1.unit));
    assert(d1.samplingInterval == 200);
    assert(isempty(d1.offset));
    
    d1.label = 'foo';
    d1.unit = 'mV';
    d1.samplingInterval = 325;
    d1.offset = 500;

    assert(strcmp(d1.label, 'foo'));
    assert(strcmp(d1.unit, 'mV'));
    assert(d1.samplingInterval == 325);
    assert(d1.offset == 500);
    
    d1.label = '';
    d1.unit = '';
    d1.offset = 0;
    
    assert(isempty(d1.label));
    assert(isempty(d1.unit));
    assert(d1.samplingInterval == 325);
    assert(d1.offset == 0);
end

function [] = test_range_dimension( varargin )
%% Test: range dimension
    f = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    b = f.createBlock('daTestBlock', 'test nixBlock');
    da = b.create_data_array('daTest', 'test nixDataArray', 'double', [1 2]);
    ticks = [1 2 3 4];
    d1 = da.append_range_dimension(ticks);

    assert(strcmp(d1.dimensionType, 'range'));
    assert(isempty(d1.label));
    assert(isempty(d1.unit));
    assert(isequal(d1.ticks, ticks));
    
    new_ticks = [5 6 7 8];
    d1.label = 'foo';
    d1.unit = 'mV';
    d1.ticks = new_ticks;
    
    assert(strcmp(d1.label, 'foo'));
    assert(strcmp(d1.unit, 'mV'));
    assert(isequal(d1.ticks, new_ticks));
    
    d1.label = '';
    d1.unit = '';
    
    assert(isempty(d1.label));
    assert(isempty(d1.unit));
end