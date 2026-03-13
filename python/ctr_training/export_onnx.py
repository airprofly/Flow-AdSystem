#!/usr/bin/env python3
"""
ONNX 模型导出脚本
将训练好的 PyTorch DeepFM 模型导出为 ONNX 格式

Author: airprofly
Date: 2026-03-12
"""

import torch
import argparse
import os
import sys
from pathlib import Path

# 添加项目路径
sys.path.insert(0, str(Path(__file__).parent))

# 导入模型定义
from train_with_synthetic_data import DeepFM


def export_model_to_onnx(
    checkpoint_path: str,
    output_path: str,
    opset_version: int = 14,
    verbose: bool = True,
    field_dims=None,
    embed_dim=32,
    mlp_dims=None,
    dropout=0.2,
    num_dense_features=13
):
    """
    将 PyTorch 模型导出为 ONNX 格式

    Args:
        checkpoint_path: PyTorch 检查点文件路径
        output_path: 输出的 ONNX 文件路径
        opset_version: ONNX opset 版本
        verbose: 是否打印详细信息
        field_dims: 稀疏特征维度列表
        embed_dim: 嵌入维度
        mlp_dims: MLP 隐藏层维度
        dropout: Dropout 比例
        num_dense_features: 连续特征数量
    """
    print(f"📦 加载模型检查点: {checkpoint_path}")

    # 检查文件是否存在
    if not os.path.exists(checkpoint_path):
        raise FileNotFoundError(f"检查点文件不存在: {checkpoint_path}")

    # 默认参数
    if mlp_dims is None:
        mlp_dims = [256, 128, 64]

    # 加载检查点
    checkpoint = torch.load(checkpoint_path, map_location='cpu')

    # 获取模型配置
    if isinstance(checkpoint, dict):
        if 'model_state_dict' in checkpoint:
            model_state = checkpoint['model_state_dict']
            # 尝试从检查点中提取配置信息
            field_dims = checkpoint.get('field_dims', field_dims)
            embed_dim = checkpoint.get('embed_dim', embed_dim)
            mlp_dims = checkpoint.get('mlp_dims', mlp_dims)
            dropout = checkpoint.get('dropout', dropout)
            num_dense_features = checkpoint.get('num_dense_features', num_dense_features)
        else:
            model_state = checkpoint
    else:
        model_state = checkpoint

    # 如果没有提供 field_dims,使用默认值
    if field_dims is None:
        # 默认: 26 个稀疏特征,每个特征 10000 个可能值
        field_dims = [10000] * 26

    print(f"📊 模型配置:")
    print(f"  - 稀疏特征数: {len(field_dims)}")
    print(f"  - 连续特征数: {num_dense_features}")
    print(f"  - 嵌入维度: {embed_dim}")
    print(f"  - MLP 隐藏层: {mlp_dims}")

    # 创建模型
    model = DeepFM(
        field_dims=field_dims,
        embed_dim=embed_dim,
        mlp_dims=mlp_dims,
        dropout=dropout,
        num_dense_features=num_dense_features
    )
    model.load_state_dict(model_state)
    model.eval()

    print("✅ 模型加载成功")

    # 创建虚拟输入
    batch_size = 1
    # 确保虚拟输入在有效范围内
    dummy_sparse = torch.zeros((batch_size, len(field_dims)), dtype=torch.long)
    for i, dim in enumerate(field_dims):
        dummy_sparse[0, i] = torch.randint(0, dim, (1,)).item()
    dummy_dense = torch.randn(batch_size, num_dense_features, dtype=torch.float32)

    print(f"🔧 准备导出 ONNX...")
    print(f"  - Opset 版本: {opset_version}")
    print(f"  - 批次大小: {batch_size}")

    # 导出模型
    torch.onnx.export(
        model,
        (dummy_sparse, dummy_dense),
        output_path,
        dynamo=False,
        export_params=True,          # 存储训练好的参数权重
        opset_version=opset_version,  # ONNX 版本
        do_constant_folding=True,     # 是否执行常量折叠优化
        input_names=['sparse_features', 'dense_features'],
        output_names=['ctr_prediction'],
        dynamic_axes={
            'sparse_features': {0: 'batch_size'},  # 可变长度批次
            'dense_features': {0: 'batch_size'},
            'ctr_prediction': {0: 'batch_size'}
        },
        verbose=verbose,
        # 保持向后兼容
        keep_initializers_as_inputs=False
    )

    print(f"✅ ONNX 模型已导出到: {output_path}")

    # 验证导出的模型
    print("\n🔍 验证 ONNX 模型...")
    try:
        import onnx
        import onnxruntime as ort

        # 加载并检查 ONNX 模型
        onnx_model = onnx.load(output_path)
        onnx.checker.check_model(onnx_model)
        print("✅ ONNX 模型检查通过")

        # 创建 ONNX Runtime 会话
        sess_options = ort.SessionOptions()
        sess_options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL

        session = ort.InferenceSession(
            output_path,
            sess_options,
            providers=['CPUExecutionProvider']
        )

        # 获取输入输出信息
        print("\n📋 模型输入输出信息:")
        for input_meta in session.get_inputs():
            print(f"  输入: {input_meta.name}, 形状: {input_meta.shape}, 类型: {input_meta.type}")

        for output_meta in session.get_outputs():
            print(f"  输出: {output_meta.name}, 形状: {output_meta.shape}, 类型: {output_meta.type}")

        # 测试推理
        print("\n🧪 测试 ONNX Runtime 推理...")

        # 准备测试数据
        import numpy as np
        # 确保测试数据在有效范围内
        test_sparse = np.zeros((1, len(field_dims)), dtype=np.int64)
        for i, dim in enumerate(field_dims):
            test_sparse[0, i] = np.random.randint(0, dim)
        test_dense = np.random.randn(1, num_dense_features).astype(np.float32)

        # PyTorch 推理
        with torch.no_grad():
            torch_output = model(
                torch.from_numpy(test_sparse),
                torch.from_numpy(test_dense)
            )
            torch_ctr = torch_output.numpy()[0]

        # ONNX Runtime 推理
        onnx_output = session.run(
            None,
            {
                'sparse_features': test_sparse,
                'dense_features': test_dense
            }
        )
        onnx_ctr = onnx_output[0][0]

        print(f"  PyTorch 输出: {torch_ctr:.6f}")
        print(f"  ONNX 输出:   {onnx_ctr:.6f}")
        print(f"  差异:        {abs(torch_ctr - onnx_ctr):.10f}")

        if abs(torch_ctr - onnx_ctr) < 1e-5:
            print("✅ ONNX 推理结果验证通过!")
        else:
            print("⚠️  警告: ONNX 和 PyTorch 输出存在差异")

        # 打印模型大小
        model_size_mb = os.path.getsize(output_path) / (1024 * 1024)
        print(f"\n📏 模型大小: {model_size_mb:.2f} MB")

    except ImportError as e:
        print(f"⚠️  警告: 无法导入 ONNX 验证库: {e}")
        print("   请安装: pip install onnx onnxruntime")

    return output_path


def main():
    parser = argparse.ArgumentParser(description='导出 DeepFM 模型为 ONNX 格式')
    parser.add_argument(
        '--checkpoint',
        type=str,
        default='checkpoints/best_model.pt',
        help='PyTorch 检查点文件路径'
    )
    parser.add_argument(
        '--output',
        type=str,
        default='models/deep_fm.onnx',
        help='输出的 ONNX 文件路径'
    )
    parser.add_argument(
        '--opset',
        type=int,
        default=14,
        help='ONNX opset 版本'
    )
    parser.add_argument(
        '--no-verbose',
        action='store_true',
        help='不打印详细的导出信息'
    )

    args = parser.parse_args()

    # 创建输出目录
    output_dir = os.path.dirname(args.output)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    # 导出模型
    try:
        export_model_to_onnx(
            checkpoint_path=args.checkpoint,
            output_path=args.output,
            opset_version=args.opset,
            verbose=not args.no_verbose
        )
        print("\n🎉 导出成功!")
        return 0
    except Exception as e:
        print(f"\n❌ 导出失败: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
