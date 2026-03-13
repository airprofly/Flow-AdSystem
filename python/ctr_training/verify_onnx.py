#!/usr/bin/env python3
"""
验证 ONNX 模型推理
使用与 C++ 完全相同的特征值进行测试
"""

import numpy as np
import onnxruntime as ort

def test_onnx_model():
    print("🧪 测试 ONNX 模型推理...")
    print()

    # 加载模型
    model_path = "models/deep_fm.onnx"
    print(f"📦 加载模型: {model_path}")

    try:
        session = ort.InferenceSession(model_path)
        print("✅ 模型加载成功")
    except Exception as e:
        print(f"❌ 模型加载失败: {e}")
        return False

    # 获取输入输出信息
    print()
    print("📋 模型输入输出信息:")
    for input_meta in session.get_inputs():
        print(f"  输入: {input_meta.name}, 形状: {input_meta.shape}, 类型: {input_meta.type}")

    for output_meta in session.get_outputs():
        print(f"  输出: {output_meta.name}, 形状: {output_meta.shape}, 类型: {output_meta.type}")

    # 准备测试数据 (使用 C++ 输出的特征值)
    print()
    print("📊 准备测试数据 (与 C++ 完全一致):")

    # 稀疏特征 (13个)
    sparse_features = np.array([[
        990855,   # ad_id
        88711,    # campaign_id
        25,       # industry_id
        1,        # creative_type
        31,       # ad_size
        0,        # user_gender
        1001,     # user_city
        0,        # user_device_type
        3,        # day_of_week
        0,        # is_weekend
        101,      # page_category
        4,        # network_type
        1         # carrier
    ]], dtype=np.int64)

    # 连续特征 (8个)
    dense_features = np.array([[
        0.08,     # ad_hist_ctr
        0.02,     # ad_hist_cvr
        0.1,      # ad_position
        0.35,     # user_age_norm
        0.05,     # user_history_ctr
        6.90875,  # user_history_imp
        0.913043, # hour_norm
        3.0       # time_slot
    ]], dtype=np.float32)

    print("稀疏特征:", sparse_features[0])
    print("连续特征:", dense_features[0])

    # 运行推理
    print()
    print("🚀 执行推理...")

    try:
        result = session.run(None, {
            'sparse_features': sparse_features,
            'dense_features': dense_features
        })

        ctr = result[0][0]
        print(f"✅ 推理成功!")
        print(f"   CTR 预估值: {ctr:.6f}")
        print(f"   eCPM (假设出价 100): {ctr * 100:.2f} 元/千次")
        return True

    except Exception as e:
        print(f"❌ 推理失败: {e}")
        print()
        print("💡 可能的原因:")
        print("   1. 特征值超出嵌入层的有效范围")
        print("   2. 模型输入输出名称不匹配")
        print("   3. ONNX 模型文件损坏")
        return False

if __name__ == '__main__':
    import sys
    sys.path.insert(0, 'python/ctr_training')
    test_onnx_model()
