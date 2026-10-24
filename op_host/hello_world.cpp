#include "../op_kernel/hello_world_tiling.h"
#include "register/op_def_registry.h"

namespace optiling {
    const uint32_t BLOCK_DIM = 8;   // 用 8 个 AI Core
    const uint32_t TILE_NUM = 8;    // 每核再切 8 块

    static ge::graphStatus TilingFunc(gert::TilingContext* context)
    {
        HelloWorldTilingData* tiling = context->GetTilingData<HelloWorldTilingData>();

        uint32_t totalLength =
            static_cast<uint32_t>(context->GetInputShape(0)->GetOriginShape().GetShapeSize());

        tiling->totalLength = totalLength;
        tiling->tileNum = TILE_NUM;

        // 不设长度，kernel 会收到 0 字节 tiling，输出全是 0
        context->GetRawTilingData()->SetDataSize(sizeof(HelloWorldTilingData));

        context->SetBlockDim(BLOCK_DIM);
        context->GetWorkspaceSizes(1)[0] = 0;
        return ge::GRAPH_SUCCESS;
    }
}

namespace ge {
    static ge::graphStatus InferShape(gert::InferShapeContext* context)
    {
        *context->GetOutputShape(0) = *context->GetInputShape(0);
        return GRAPH_SUCCESS;
    }

    static ge::graphStatus InferDataType(gert::InferDataTypeContext* context)
    {
        context->SetOutputDataType(0, context->GetInputDataType(0));
        return ge::GRAPH_SUCCESS;
    }
}

namespace ops {
    class   HelloWorld : public OpDef {
    public:
        explicit HelloWorld(const char* name) : OpDef(name)
        {
            this->Input("x")
                .ParamType(REQUIRED)
                .DataType({ge::DT_FLOAT16})
                .Format({ge::FORMAT_ND})
                .UnknownShapeFormat({ge::FORMAT_ND});
            this->Output("y")
                .ParamType(REQUIRED)
                .DataType({ge::DT_FLOAT16})
                .Format({ge::FORMAT_ND})
                .UnknownShapeFormat({ge::FORMAT_ND});

            this->SetInferShape(ge::InferShape).SetInferDataType(ge::InferDataType);
            this->AICore().SetTiling(optiling::TilingFunc);
            this->AICore().AddConfig("ascend910b");
        }
    };
    OP_ADD(HelloWorld);
}