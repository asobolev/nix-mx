#include <iostream>
#include <math.h>
#include <string>
#include <vector>
#include "mex.h"

#include <nix.hpp>

// *** datatype converter

template<typename T>
struct to_mx_class_id {

    static mxClassID value() {
        nix::DataType dtype = nix::to_data_type<T>::value;
        switch (dtype) {
            case nix::DataType::Double:
                return mxDOUBLE_CLASS;

            default:
                mexErrMsgIdAndTxt("MATLAB:toclassid:notimplemented", "Implement me!");
                return mxVOID_CLASS;
        }
    }

};


template<typename T>
mxArray* vector_to_array(const std::vector<T> &v) {
    mxClassID klass_id = to_mx_class_id<T>::value();
    mxArray *data = mxCreateNumericMatrix(1, v.size(), klass_id, mxREAL);
    double *ptr = mxGetPr(data);
    memcpy(ptr, v.data(), sizeof(T) * v.size());
    return data;
}


// *** nix entities holder ***

template<typename T>
struct entity_to_id { };

template<>
struct entity_to_id<nix::File> {
    static constexpr int value() { return 1; }
};

template<>
struct entity_to_id<nix::Block> {
    static constexpr int value() { return 2; }
};

template<>
struct entity_to_id<nix::DataArray> {
    static constexpr int value() { return 3; }
};



class handle {
public:

    template<typename T>
    handle(const T &obj) : et(new cell<T>(obj)) { }
    handle(uint64_t h) : et(reinterpret_cast<entity *>(h)) { }

    template<typename T>
    T get() const {
        if (et == nullptr) {
            throw std::runtime_error("called get on empty handle");
        }

        int eid = entity_to_id<T>::value();
        if (eid != et->id) {
            throw std::runtime_error("tried to get a entity of wrong type");
        }

        return dynamic_cast<cell<T> *>(et)->obj;
    }

    uint64_t address() const {
        return reinterpret_cast<uint64_t >(et);
    }

    void destroy() {
        et->id = 0;
        et->destory();
        delete et;
        et = nullptr;
    }

private:
    struct entity {

        template<typename T>
        entity(const T &e) : id(entity_to_id<T>::value()) { }

        int id;

        virtual void destory() = 0;

        virtual ~entity() { }
    };

    template<typename T>
    struct cell : public entity {
        cell(const T &obj) : entity(obj), obj(obj) { }

        virtual void destory() override {
            obj = nix::none;
        }

        T obj;
    };


    entity *et;
};


// *** argument helpers ***

template<typename T>
class argument_helper {
public:

    argument_helper(T **arg, size_t n) : array(arg), number(n) { }

    bool check_size(int pos, bool fatal = false) const {
        bool res = pos + 1 > number;
        return res;
    }

    bool require_arguments(const std::vector<mxClassID> &args, bool fatal = false) const {
        bool res = true;
        if (args.size() > number) {
            res = false;
        } else {
            for (size_t i = 0; i < args.size(); i++) {
                if (mxGetClassID(array[i]) != args[i]) {
                    res = false;
                    break;
                }
            }
        }

        if (!res && fatal) {
            mexErrMsgIdAndTxt("MATLAB:args:numortype", "Wrong number or types of arguments.");
        }

        return res;
    }

protected:
    T **array;
    size_t number;
};

class extractor : public argument_helper<const mxArray> {
public:
    extractor(const mxArray **arr, int n) : argument_helper(arr, n) { }

    std::string str(int pos) const {
        //make sure it is a string
        char *tmp = mxArrayToString(array[pos]);
        std::string the_string(tmp);
        mxFree(tmp);
        return the_string;
    }


    uint64_t uint64(int pos) const {
        const void *data = mxGetData(array[pos]);
        uint64_t res;
        memcpy(&res, data, sizeof(uint64_t));
        return res;
    }

    template<typename T>
    T entity(int pos) const {
        return hdl(pos).get<T>();
    }

    handle hdl(int pos) const {
        uint64_t address = uint64(pos);
        handle h(address);
        return h;
    }

    mxClassID class_id(int pos) const {
        return mxGetClassID(array[pos]);
    }

    bool is_str(int pos) const {
        mxClassID category = class_id(pos);
        return category == mxCHAR_CLASS;
    }

private:
};


class infusor : public argument_helper<mxArray> {
public:
    infusor(mxArray **arr, int n) : argument_helper(arr, n) { }

    void set(int pos, std::string str) {
        if (check_size(pos)) {
            return;
        }

        array[pos] = mxCreateString(str.c_str());
    }

    void set(int pos, uint64_t v) {
        array[pos] = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL);
        void *pointer = mxGetPr(array[pos]);
        memcpy(pointer, &v, sizeof(v));
    }

    void set(int pos, mxArray *arr) {
        array[pos] = arr;
    }

    void set(int pos, const handle &h) {
        set(pos, h.address());
    }

private:
};

// *** functions ***

static void entity_destory(const extractor &input, infusor &output)
{
    mexPrintf("[+] entity_destory\n");
    handle h = input.hdl(1);
    h.destroy();
}

static void open_file(const extractor &input, infusor &output)
{
    input.require_arguments({mxCHAR_CLASS, mxCHAR_CLASS}, true);
    mexPrintf("[+] open_file\n");

    std::string name = input.str(1);

    nix::File fn = nix::File::open(name, nix::FileMode::ReadWrite);
    handle h = handle(fn);

    output.set(0, h);
}

static void list_blocks(const extractor &input, infusor &output)
{
    mexPrintf("[+] list_blocks\n");
    nix::File fd = input.entity<nix::File>(1);

    std::vector<nix::Block> blocks = fd.blocks();

    std::vector<const char *> fields = { "name", "id" };

    mxArray *sa =  mxCreateStructMatrix(blocks.size(), 1, fields.size(), fields.data());

    for (size_t n = 0; n < blocks.size(); n++) {
        mxSetFieldByNumber(sa, n, 0, mxCreateString(blocks[n].name().c_str()));
        mxSetFieldByNumber(sa, n, 1, mxCreateString(blocks[n].id().c_str()));
    }

    output.set(0, sa);
}

static void list_data_arrays(const extractor &input, infusor &output)
{
    mexPrintf("[+] list_data_arrays\n");

    nix::File nf = input.entity<nix::File>(1);
    nix::Block block = nf.getBlock(input.str(2));

    std::vector<nix::DataArray> arr = block.dataArrays();

    std::vector<const char *> fields = { "name", "id" };

    mxArray *sa =  mxCreateStructMatrix(arr.size(), 1, fields.size(), fields.data());

    for (size_t n = 0; n < arr.size(); n++) {
        mxSetFieldByNumber(sa, n, 0, mxCreateString(arr[n].name().c_str()));
        mxSetFieldByNumber(sa, n, 1, mxCreateString(arr[n].id().c_str()));
    }

    output.set(0, sa);
}

static void open_block(const extractor &input, infusor &output)
{
    mexPrintf("[+] list_data_arrays\n");
    nix::File nf = input.entity<nix::File>(1);
    nix::Block block = nf.getBlock(input.str(2));
    handle bb = handle(block);
    output.set(0, bb);
}

static void open_data_array(const extractor &input, infusor &output)
{
    mexPrintf("[+] list_data_arrays\n");
    nix::Block block = input.entity<nix::Block>(1);
    nix::DataArray da = block.getDataArray(input.str(2));
    handle bd = handle(da);
    output.set(0, bd);
}


static mxArray * ndsize_to_mxarray(const nix::NDSize &size)
{
    mxArray *res = mxCreateNumericMatrix(1, size.size(), mxUINT64_CLASS, mxREAL);
    void *ptr = mxGetData(res);
    uint64_t *data = static_cast<uint64_t *>(ptr);

    for (size_t i = 0; i < size.size(); i++) {
        data[i] = static_cast<uint64_t>(size[i]);
    }

    return res;
}

static nix::NDSize mx_array_to_ndsize(const mxArray *arr) {

    size_t m = mxGetM(arr);
    size_t n = mxGetN(arr);

    //if (m != 1 && n != 1)

    size_t k = std::max(n, m);
    nix::NDSize size(k);

    double *data = mxGetPr(arr);
    for (size_t i = 0; i < size.size(); i++) {
        size[i] = static_cast<nix::NDSize::value_type>(data[i]);
    }

    return size;
}

static mxArray *nmCreateScalar(uint32_t val) {
    mxArray *arr = mxCreateNumericMatrix(1, 1, mxUINT32_CLASS, mxREAL);
    void *data = mxGetData(arr);
    memcpy(data, &val, sizeof(uint32_t));
    return arr;
}

static mxArray *dim_to_struct(nix::SetDimension dim) {

    std::vector<const char *> fields = { "type", "type_id", "label" };
    mxArray *sa =  mxCreateStructMatrix(1, 1, fields.size(), fields.data());

    mxSetFieldByNumber(sa, 0, 0, mxCreateString("set"));
    mxSetFieldByNumber(sa, 0, 1, nmCreateScalar(1));

    std::vector<std::string> labels = dim.labels();
    return sa;
}


static mxArray *dim_to_struct(nix::SampledDimension dim) {

    std::vector<const char *> fields = { "type", "type_id", "interval", "label", "unit"};
    mxArray *sa =  mxCreateStructMatrix(1, 1, fields.size(), fields.data());

    mxSetFieldByNumber(sa, 0, 0, mxCreateString("sampled"));
    mxSetFieldByNumber(sa, 0, 1, nmCreateScalar(2));
    mxSetFieldByNumber(sa, 0, 2, mxCreateDoubleScalar(dim.samplingInterval()));

    boost::optional<std::string> label = dim.label();
    if (label) {
        mxSetFieldByNumber(sa, 0, 3, mxCreateString(label->c_str()));
    }

    boost::optional<std::string> unit = dim.unit();
    if (unit) {
        mxSetFieldByNumber(sa, 0, 4, mxCreateString(unit->c_str()));
    }

    return sa;
}

static mxArray *dim_to_struct(nix::RangeDimension dim) {

    std::vector<const char *> fields = { "type", "type_id" };
    mxArray *sa =  mxCreateStructMatrix(1, 1, fields.size(), fields.data());

    mxSetFieldByNumber(sa, 0, 0, mxCreateString("range"));
    mxSetFieldByNumber(sa, 0, 1, nmCreateScalar(3));
    return sa;
}

static void data_array_describe(const extractor &input, infusor &output)
{
    mexPrintf("[+] block_describe_data_array\n");
    nix::DataArray da = input.entity<nix::DataArray>(1);

    std::vector<const char *> fields = { "name", "id", "shape",  "unit", "dimensions", "label",
                                         "polynom_coefficients"};
    mxArray *sa =  mxCreateStructMatrix(1, 1, fields.size(), fields.data());

    mxSetFieldByNumber(sa, 0, 0, mxCreateString(da.name().c_str()));
    mxSetFieldByNumber(sa, 0, 1, mxCreateString(da.id().c_str()));
    mxSetFieldByNumber(sa, 0, 2, ndsize_to_mxarray(da.dataExtent()));

    boost::optional<std::string> unit = da.unit();
    if (unit) {
        mxSetFieldByNumber(sa, 0, 3, mxCreateString(unit->c_str()));
    }

    size_t ndims = da.dimensionCount();

    mxArray *dims = mxCreateCellMatrix(1, ndims);
    std::vector<nix::Dimension> da_dims = da.dimensions();

    for(size_t i = 0; i < ndims; i++) {
        mxArray *ca;

        switch(da_dims[i].dimensionType()) {
            case nix::DimensionType::Set:
                ca = dim_to_struct(da_dims[i].asSetDimension());
                break;
            case nix::DimensionType::Range:
                ca = dim_to_struct(da_dims[i].asRangeDimension());
                break;
            case nix::DimensionType::Sample:
                ca = dim_to_struct(da_dims[i].asSampledDimension());
                break;
        }


        mxSetCell(dims, i, ca);
    }

    mxSetFieldByNumber(sa, 0, 4, dims);


    boost::optional<std::string> label = da.label();
    if (unit) {
        mxSetFieldByNumber(sa, 0, 5, mxCreateString(label->c_str()));
    }

    std::vector<double> pc = da.polynomCoefficients();

    if (!pc.empty()) {
        mxSetFieldByNumber(sa, 0, 6, vector_to_array(pc));
    }

    output.set(0, sa);
}

static void data_array_read_all(const extractor &input, infusor &output)
{
    mexPrintf("[+] block_describe_data_array\n");
    nix::DataArray da = input.entity<nix::DataArray>(1);

    nix::NDSize size = da.dataExtent();
    std::vector<mwSize> dims(size.size());

    for (size_t i = 0; i < size.size(); i++) {
        dims[i] = static_cast<mwSize>(size[i]);
    }

    mxArray *data = mxCreateNumericArray(size.size(), dims.data(), mxDOUBLE_CLASS, mxREAL);
    double *ptr = mxGetPr(data);

    nix::NDSize offset(size.size(), 0);
    da.getData(nix::DataType::Double , ptr, size, offset);

    output.set(0, data);
}

// *** ***

typedef void (*fn_t)(const extractor &input, infusor &output);

struct fendpoint {

fendpoint(std::string name, fn_t fn) : name(name), fn(fn) {}

    std::string name;
    fn_t fn;
};

const std::vector<fendpoint> funcs = {
        {"Entity::destroy", entity_destory},
        {"File::open", open_file},
        {"File::listBlocks", list_blocks},
        {"File::listDataArrays", list_data_arrays},
        {"File::openBlock", open_block},
        {"Block::openDataArray", open_data_array},
        {"DataArray::describe", data_array_describe},
        {"DataArray::readAll", data_array_read_all}
};

// main entry point
void mexFunction(int            nlhs,
                 mxArray       *lhs[],
                 int            nrhs,
                 const mxArray *rhs[])
{
    extractor input(rhs, nrhs);
    infusor   output(lhs, nlhs);

    std::string cmd = input.str(0);

    mexPrintf("[F] %s\n", cmd.c_str());

    bool processed = false;
    for (const auto &fn : funcs) {
        if (fn.name == cmd) {
            try {
                fn.fn(input, output);
            } catch (std::exception &e) {
                mexErrMsgIdAndTxt("MATLAB:arg:dispatch", e.what());
            } catch (...) {
                mexErrMsgIdAndTxt("MATLAB:arg:dispatch", "unkown exception");
            }
            processed = true;
            break;
        }
    }

    if (!processed) {
        mexErrMsgIdAndTxt("MATLAB:arg:dispatch", "Unkown command");
    }
}

