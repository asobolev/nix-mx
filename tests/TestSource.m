function funcs = testSource
%TESTSOURCE tests for Source
%   Detailed explanation goes here

    funcs = {};
    funcs{end+1} = @test_create_source;
    funcs{end+1} = @test_delete_source;
    funcs{end+1} = @test_attrs;
    funcs{end+1} = @test_fetch_sources;
    funcs{end+1} = @test_open_source;
    funcs{end+1} = @test_set_metadata;
    funcs{end+1} = @test_open_metadata;
end

%% Test: fetch sources
function [] = test_fetch_sources( varargin )
    test_file = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    getBlock = test_file.createBlock('sourcetest', 'nixBlock');
    getSource = getBlock.create_source('sourcetest', 'nixSource');
    tmp = getSource.create_source('nestedsource1', 'nixSource');
    tmp = getSource.create_source('nestedsource2', 'nixSource');
    
    assert(size(getSource.sources, 1) == 2);
end

%% Test: Open source by ID or name
function [] = test_open_source( varargin )

    test_file = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    getBlock = test_file.createBlock('sourcetest', 'nixBlock');
    getSource = getBlock.create_source('sourcetest', 'nixSource');
    assert(isempty(getSource.sources));

    sourceName = 'nestedsource';
    createSource = getSource.create_source(sourceName, 'nixSource');
    getSourceByID = getSource.open_source(createSource.id);
    assert(~isempty(getSourceByID));

    getSourceByName = getSource.open_source(sourceName);
    assert(~isempty(getSourceByName));

    %-- test open non existing source
    getNonSource = getSource.open_source('I dont exist');
    assert(isempty(getNonSource));
end

%% Test: Set metadata
function [] = test_set_metadata ( varargin )
    f = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    tmp = f.createSection('testSection1', 'nixSection');
    tmp = f.createSection('testSection2', 'nixSection');
    b = f.createBlock('testBlock', 'nixBlock');
    s = b.create_source('testSource', 'nixSource');

    assert(isempty(s.open_metadata));
    s.set_metadata(f.sections{1});
    assert(strcmp(s.open_metadata.name, 'testSection1'));
    s.set_metadata(f.sections{2});
    assert(strcmp(s.open_metadata.name, 'testSection2'));
    s.set_metadata('');
    assert(isempty(s.open_metadata));
end

%% Test: Open metadata
function [] = test_open_metadata( varargin )
    f = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    tmp = f.createSection('testSection', 'nixSection');
    b = f.createBlock('testBlock', 'nixBlock');
    s = b.create_source('testSource', 'nixSource');
    s.set_metadata(f.sections{1});

    assert(strcmp(s.open_metadata.name, 'testSection'));
end

%% Test: create source
function [] = test_create_source ( varargin )
    test_file = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    getBlock = test_file.createBlock('sourcetest', 'nixBlock');
    getSource = getBlock.create_source('sourcetest', 'nixSource');
    assert(isempty(getSource.sources));

    createSource = getSource.create_source('nestedsource', 'nixSource');
    assert(~isempty(getSource.sources));
    assert(strcmp(createSource.name, 'nestedsource'));
    assert(strcmp(createSource.type, 'nixSource'));
end

%% Test: delete source
function [] = test_delete_source( varargin )
    test_file = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    getBlock = test_file.createBlock('sourcetest', 'nixBlock');
    getSource = getBlock.create_source('sourcetest', 'nixSource');
    assert(isempty(getSource.sources));

    tmp = getSource.create_source('nestedsource1', 'nixSource');
    tmp = getSource.create_source('nestedsource2', 'nixSource');
    assert(getSource.delete_source('nestedsource1'));
    assert(getSource.delete_source(getSource.sources{1}.id));
    assert(~getSource.delete_source('I do not exist'));
    assert(isempty(getSource.sources));
end

function [] = test_attrs( varargin )
%% Test: Access Attributes
    f = nix.File(fullfile(pwd, 'tests', 'testRW.h5'), nix.FileMode.Overwrite);
    b = f.createBlock('tagtest', 'test nixBlock');
    s = b.create_source('sourcetest', 'test nixSource');

    assert(~isempty(s.id));
    assert(strcmp(s.name, 'sourcetest'));
    assert(strcmp(s.type, 'test nixSource'));
    
    s.type = 'nixSource';
    assert(strcmp(s.type, 'nixSource'));
    
    assert(isempty(s.definition));
    s.definition = 'source definition';
    assert(strcmp(s.definition, 'source definition'));

    s.definition = '';
    assert(isempty(s.definition));
end
