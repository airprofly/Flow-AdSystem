#!/usr/bin/env python3
"""
简化的 ONNX 模型验证脚本
不依赖 onnx 包,只使用 onnxruntime
"""

import numpy as np
import sys

try:
    import onnxruntime as ort
except ImportError:
    print("❌ 错误: 需要安装 onnxruntime")
    print("   请运行: pip install onnxruntime")
    sys.exit(1)

def main():
    print("=" * 60)
    print("  ONNX 模型验证")
    print("=" * 60)

    model_path = "models/deep_fm.onnx"

    try:
        # 创建 ONNX Runtime 会话
        print(f"\n📦 加载模型: {model_path}")
        session = ort.InferenceSession(
            model_path,
            providers=['CPUExecutionProvider']
        )
        print("✅ 模型加载成功")

        # 获取输入信息
        print("\n📋 模型输入信息:")
        for input_meta in session.get_inputs():
            print(f"  名称: {input_meta.name}")
            print(f"  形状: {input_meta.shape}")
            print(f"  类型: {input_meta.type}")

        # 获取输出信息
        print("\n📋 模型输出信息:")
        for output_meta in session.get_outputs():
            print(f"  名称: {output_meta.name}")
            print(f"  形状: {output_meta.shape}")
            print(f"  类型: {output_meta.type}")

        # 准备测试数据 (与 C++ 代码一致)
        print("\n🔧 准备测试数据...")

        # 稀疏特征 (13个) - 与 C++ 代码中的特征顺序一致
        sparse_data = np.array([[
            123456,    # ad_id
            12345,     # campaign_id
            25,        # industry_id
            1,         # creative_type
            10,        # ad_size
            0,         # user_gender
            1001,      # user_city
            0,         # user_device_type
            3,         # day_of_week
            0,         # is_weekend
            101,       # page_category
            4,         # network_type
            1          # carrier
        ]], dtype=np.int64)

        print(f"  稀疏特征形状: {sparse_data.shape}")
        print(f"  稀疏特征类型: {sparse_data.dtype}")
        print(f"  稀疏特征值: {sparse_data[0]}")

        # 连续特征 (8个)
        dense_data = np.array([[
            0.08,   # ad_hist_ctr
            0.02,   # ad_hist_cvr
            0.1,    # ad_position
            0.35,   # user_age_norm
            0.05,   # user_history_ctr
            6.9,    # user_history_imp
            0.91,   # hour_norm
            3.0     # time_slot
        ]], dtype=np.float32)

        print(f"  连续特征形状: {dense_data.shape}")
        print(f"  连续特征类型: {dense_data.dtype}")
        print(f"  连续特征值: {dense_data[0]}")

        # 执行推理
        print("\n🚀 执行推理...")
        try:
            outputs = session.run(
                None,
                {
                    'sparse_features': sparse_data,
                    'dense_features': dense_data
                }
            )

            ctr = outputs[0][0]
            print(f"✅ 推理成功!")
            print(f"  CTR 预估值: {ctr:.6f}")

            # 验证 CTR 值范围
            if 0.0 <= ctr <= 1.0:
                print(f"  ✅ CTR 值在有效范围内 [0, 1]")
            else:
                print(f"  ⚠️  警告: CTR 值超出有效范围")

        except Exception as e:
            print(f"❌ 推理失败: {e}")
            import traceback
            traceback.print_exc()
            return 1

        print("\n" + "=" * 60)
        print("  🎉 验证通过!")
        print("=" * 60)
        return 0

    except Exception as e:
        print(f"\n❌ 错误: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == '__main__':
    sys.exit(main())
