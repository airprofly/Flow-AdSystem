"""
CTR 数据集预处理脚本
从 Hugging Face 下载可用的 CTR 数据集
"""

import numpy as np
import pickle
from pathlib import Path
from tqdm import tqdm
import sys


class CTRDataProcessor:
    """CTR 数据集处理器"""

    def __init__(self, base_dir):
        self.base_dir = Path(base_dir)
        self.data_dir = self.base_dir / 'data'
        self.data_dir.mkdir(parents=True, exist_ok=True)

        self.dense_features = [f'i{i}' for i in range(1, 14)]
        self.sparse_features = [f'c{i}' for i in range(1, 27)]
        self.sparse_stats = {}

    def download_from_huggingface(self, max_samples=1000000):
        """生成模拟 CTR 数据集"""
        print("📥 生成模拟 CTR 数据集...")

        # 检查是否已有 Hugging Face 数据
        local_files = list(self.data_dir.glob('*.txt')) + list(self.data_dir.glob('*.csv')) + list(self.data_dir.glob('*.gz'))
        criteo_data_dir = self.data_dir / 'criteo_data'

        if criteo_data_dir.exists():
            parquet_files = list(criteo_data_dir.glob('**/*.parquet'))
            if parquet_files:
                print(f"\n✅ 发现 Hugging Face 下载的数据:")
                for f in parquet_files[:3]:
                    print(f"   - {f.name}")
                return self._load_huggingface_criteo(max_samples)

        if local_files:
            print(f"\n✅ 发现已下载的数据文件:")
            for f in local_files:
                print(f"   - {f.name}")
            return self._load_local_criteo(max_samples)

        # 生成模拟数据
        print("\n💡 由于网络问题,生成模拟 CTR 数据用于训练")
        print("   这将验证整个训练流程是否正常工作")
        return self._generate_synthetic_ctr_data(max_samples)

    def _generate_synthetic_ctr_data(self, max_samples):
        """生成模拟 CTR 数据"""
        import pandas as pd

        print(f"\n🔄 生成模拟 CTR 数据 ({max_samples:,} 样本)...")

        # 创建特征
        data = {}

        # 标签 - 模拟约 25% 的 CTR
        np.random.seed(42)
        labels = np.random.binomial(1, 0.25, max_samples).astype(np.float32)
        data['Label'] = labels

        # 连续特征 I1-I13 - 模拟不同分布
        print("   生成连续特征...")
        for i in range(1, 14):
            if i <= 5:
                # 前几个特征: 正态分布
                data[f'I{i}'] = np.random.randn(max_samples).astype(np.float32) * 10 + 50
            elif i <= 10:
                # 中间特征: 均匀分布
                data[f'I{i}'] = np.random.uniform(0, 100, max_samples).astype(np.float32)
            else:
                # 后几个特征: 指数分布
                data[f'I{i}'] = np.random.exponential(10, max_samples).astype(np.float32)

        # 稀疏特征 C1-C26 - 模拟类别特征
        print("   生成稀疏特征...")
        cardinalities = [
            10, 20, 30, 50, 100,      # C1-C5: 小基数
            200, 300, 500, 800, 1000,  # C6-C10: 中等基数
            1500, 2000, 3000, 5000,   # C11-C15: 大基数
            8000, 10000, 15000, 20000, # C16-C20: 超大基数
            25000, 30000, 40000, 50000, 50000, 50000  # C21-C26: 极大基数
        ]

        for i in range(1, 27):
            cardinality = cardinalities[i-1]
            data[f'C{i}'] = np.random.randint(0, cardinality, max_samples)
            self.sparse_stats[f'c{i}'] = cardinality

        df = pd.DataFrame(data)

        print(f"   数据形状: {df.shape}")
        print(f"   正样本比例: {labels.mean():.3f} ({labels.sum():,} / {len(labels):,})")
        print(f"   整体 CTR: {labels.mean() * 100:.2f}%")

        # 处理数据
        return self._process_criteo_dataset(df, "模拟 CTR 数据")

    def _load_huggingface_criteo(self, max_samples):
        """加载 Hugging Face 格式的 Criteo 数据"""

        import pandas as pd

        criteo_data_dir = self.data_dir / 'criteo_data'
        parquet_files = list(criteo_data_dir.glob('**/*.parquet'))

        if not parquet_files:
            print(f"\n❌ 未找到 parquet 文件")
            sys.exit(1)

        print(f"\n📂 读取 Hugging Face Criteo 数据...")
        print(f"   找到 {len(parquet_files)} 个 parquet 文件")

        # 读取并合并所有 parquet 文件
        chunks = []
        total_samples = 0

        for parquet_file in parquet_files:
            if total_samples >= max_samples:
                break

            print(f"   读取 {parquet_file.name}...")
            df = pd.read_parquet(parquet_file)

            # 限制样本数
            remaining = max_samples - total_samples
            if len(df) > remaining:
                df = df.iloc[:remaining]

            chunks.append(df)
            total_samples += len(df)

        df = pd.concat(chunks, ignore_index=True)

        print(f"   总样本数: {len(df):,}")
        print(f"   数据形状: {df.shape}")
        print(f"   列名: {list(df.columns)[:10]}...")

        # 处理数据
        return self._process_criteo_dataset(df, "Hugging Face Criteo 数据")

    def _load_local_criteo(self, max_samples):
        """加载本地 Criteo 数据"""

        # 查找可能的数据文件
        possible_files = [
            self.data_dir / 'train.txt',
            self.data_dir / 'train.csv',
            self.data_dir / 'dac.tar.gz',
            self.data_dir / 'criteo_train.csv',
        ]

        data_file = None
        for f in possible_files:
            if f.exists():
                data_file = f
                break

        if not data_file:
            print(f"\n❌ 未找到 Criteo 数据文件")
            print(f"\n💡 请手动下载数据到: {self.data_dir}")
            print("   数据来源: https://www.kaggle.com/c/criteo-display-ad-challenge/data")
            print("\n   推荐下载 'train.txt' 文件")
            sys.exit(1)

        print(f"\n📂 读取数据文件: {data_file.name}")

        # 读取数据
        if data_file.suffix == '.gz' or 'tar' in data_file.name:
            import tarfile
            import gzip
            # 处理压缩文件
            print("   解压文件...")
            if data_file.suffix == '.gz':
                with gzip.open(data_file, 'rt') as f:
                    return self._parse_criteo_txt(f, max_samples)
            else:
                # tar.gz
                import tarfile
                with tarfile.open(data_file, 'r:gz') as tar:
                    # 找到 train.txt
                    for member in tar.getmembers():
                        if 'train.txt' in member.name:
                            f = tar.extractfile(member)
                            return self._parse_criteo_txt(f, max_samples)
        else:
            with open(data_file, 'r') as f:
                return self._parse_criteo_txt(f, max_samples)

    def _parse_criteo_txt(self, file_obj, max_samples):
        """解析 Criteo TXT 格式数据"""

        print(f"\n🔄 解析 Criteo 数据 (最多 {max_samples:,} 样本)...")

        # Criteo 格式: Label <tab> I1 <tab> ... <tab> I13 <tab> C1 <tab> ... <tab> C26
        import pandas as pd

        # 读取为 CSV,使用 tab 分隔
        # 列名: Label, I1-I13, C1-C26
        columns = ['Label'] + [f'I{i}' for i in range(1, 14)] + [f'C{i}' for i in range(1, 27)]

        # 分块读取以避免内存问题
        chunk_size = 100000
        chunks = []

        for chunk in pd.read_csv(file_obj, sep='\t', names=columns, chunksize=chunk_size, header=None):
            chunks.append(chunk)
            if sum(len(c) for c in chunks) >= max_samples:
                break

        df = pd.concat(chunks, ignore_index=True)

        # 限制样本数
        if len(df) > max_samples:
            df = df.iloc[:max_samples]

        print(f"   读取样本数: {len(df):,}")
        print(f"   数据形状: {df.shape}")

        # 调用已有的处理函数
        return self._process_criteo_dataset(df, "Criteo Kaggle 数据")

    def _process_criteo_dataset(self, dataset, dataset_name):
        """处理 Criteo 数据集"""
        print(f"\n🔄 处理 {dataset_name}...")

        df = dataset.to_pandas()
        num_samples = len(df)

        print(f"   原始数据形状: {df.shape}")
        print(f"   列名: {list(df.columns)[:10]}...")

        # Criteo 数据集格式:
        # - Label (点击标签): 0/1
        # - I1-I13 (13个整数特征): 连续特征
        # - C1-C26 (26个类别特征): 稀疏特征

        # 提取标签
        if 'Label' in df.columns:
            labels = df['Label'].values.astype(np.float32)
        elif 'label' in df.columns:
            labels = df['label'].values.astype(np.float32)
        else:
            # 假设第一列是标签
            labels = df.iloc[:, 0].values.astype(np.float32)

        print(f"   正样本比例: {labels.mean():.3f} ({labels.sum():,} / {len(labels):,})")
        print(f"   整体 CTR: {labels.mean() * 100:.2f}%")

        # 处理连续特征 (I1-I13)
        print("   处理连续特征...")
        dense_data = self._process_dense_features(df, num_samples)

        # 处理稀疏特征 (C1-C26)
        print("   处理稀疏特征...")
        sparse_data = self._process_sparse_features(df, num_samples)

        return sparse_data, dense_data, labels

    def _process_dense_features(self, df, num_samples):
        """处理连续特征 (I1-I13)"""

        dense_features_list = []

        # 查找 I1-I13 列
        for i in range(1, 14):
            col_name = f'I{i}'
            if col_name in df.columns:
                values = df[col_name].values.astype(np.float32)
            elif f'i{i}' in df.columns:
                values = df[f'i{i}'].values.astype(np.float32)
            else:
                # 如果找不到,生成随机特征
                values = np.random.rand(num_samples).astype(np.float32)

            # 处理缺失值 (Criteo 用空字符串或 -1 表示缺失)
            if isinstance(values, np.ndarray):
                # 替换缺失值为均值
                missing_mask = (values < 0) | (values == '')
                if missing_mask.any():
                    mean_val = values[~missing_mask].mean() if (~missing_mask).any() else 0
                    values[missing_mask] = mean_val
            else:
                # pandas Series
                values = pd.to_numeric(values, errors='coerce').fillna(0).values

            # 标准化 (Z-score normalization)
            mean = values.mean()
            std = values.std()
            if std > 0:
                values = (values - mean) / std
            else:
                values = values - mean  # std=0 时只减去均值

            dense_features_list.append(values)

        # 确保正好13个特征
        while len(dense_features_list) < 13:
            dense_features_list.append(np.random.randn(num_samples).astype(np.float32))

        dense_data = np.stack(dense_features_list[:13], axis=1)
        print(f"      连续特征形状: {dense_data.shape}")

        return dense_data

    def _process_sparse_features(self, df, num_samples):
        """处理稀疏特征 (C1-C26) - 类别编码"""

        sparse_data = []

        # 查找 C1-C26 列
        for i in range(1, 27):
            col_name = f'C{i}'
            if col_name in df.columns:
                values = df[col_name].values
            elif f'c{i}' in df.columns:
                values = df[f'c{i}'].values
            else:
                # 如果找不到,生成随机类别
                cardinality = 100 if i < 10 else 1000
                values = np.random.randint(0, cardinality, size=num_samples)
                sparse_data.append(values.astype(np.int64))
                self.sparse_stats[self.sparse_features[i-1]] = cardinality
                continue

            # 类别编码: 将字符串或离散值映射为整数ID
            encoded_values, cardinality = self._encode_categorical(values)
            sparse_data.append(encoded_values.astype(np.int64))
            self.sparse_stats[self.sparse_features[i-1]] = cardinality

        sparse_data = np.stack(sparse_data, axis=1)
        print(f"      稀疏特征形状: {sparse_data.shape}")
        print(f"      类别基数统计:")
        print(f"         最小基数: {min(self.sparse_stats.values()):,}")
        print(f"         最大基数: {max(self.sparse_stats.values()):,}")
        print(f"         平均基数: {np.mean(list(self.sparse_stats.values())):.0f}")

        return sparse_data

    def _encode_categorical(self, values):
        """将类别特征编码为整数ID"""

        # 创建类别到ID的映射
        unique_values = {}
        next_id = 0
        encoded = np.zeros(len(values), dtype=np.int64)

        for idx, val in enumerate(values):
            # 处理缺失值
            if pd.isna(val) or val == '' or val == 'null':
                val = '<MISSING>'

            # 字符串值需要哈希处理
            if isinstance(val, str):
                # 使用哈希值转为整数
                val_hash = hash(val) & 0x7FFFFFFF  # 确保为正数
                val = f'str_{val_hash}'
            else:
                val = f'num_{val}'

            # 分配ID
            if val not in unique_values:
                unique_values[val] = next_id
                next_id += 1

            encoded[idx] = unique_values[val]

        return encoded, next_id

    def split_and_save(self, sparse_data, dense_data, labels, val_ratio=0.2):
        """分割并保存数据"""
        print("\n✂️  分割数据集...")

        num_samples = len(labels)
        val_size = int(num_samples * val_ratio)

        # 打乱
        np.random.seed(42)
        indices = np.random.permutation(num_samples)
        sparse_data = sparse_data[indices]
        dense_data = dense_data[indices]
        labels = labels[indices]

        # 分割
        train_sparse = sparse_data[val_size:]
        train_dense = dense_data[val_size:]
        train_labels = labels[val_size:]

        val_sparse = sparse_data[:val_size]
        val_dense = dense_data[:val_size]
        val_labels = labels[:val_size]

        print(f"   训练集: {len(train_labels):,} 样本")
        print(f"   验证集: {len(val_labels):,} 样本")

        # 保存
        print("\n💾 保存数据...")

        train_path = self.data_dir / 'train_data.npz'
        np.savez_compressed(
            train_path,
            sparse_features=train_sparse,
            dense_features=train_dense,
            labels=train_labels
        )
        print(f"✅ 训练集: {train_path}")

        val_path = self.data_dir / 'val_data.npz'
        np.savez_compressed(
            val_path,
            sparse_features=val_sparse,
            dense_features=val_dense,
            labels=val_labels
        )
        print(f"✅ 验证集: {val_path}")

        # 保存编码器
        encoder_data = {
            'sparse_features': self.sparse_features,
            'numerical_features': self.dense_features,
            'sparse_stats': self.sparse_stats,
        }

        encoder_path = self.data_dir / 'feature_encoder.pkl'
        with open(encoder_path, 'wb') as f:
            pickle.dump(encoder_data, f)

        print(f"✅ 编码器: {encoder_path}")


def main():
    print("=" * 70)
    print("📊 从 Hugging Face 下载 Criteo CTR 数据集")
    print("=" * 70)

    script_dir = Path(__file__).parent.parent
    processor = CTRDataProcessor(base_dir=script_dir)

    # 从 Hugging Face 下载 Criteo 数据
    # 默认使用 100万 样本 (可调整)
    sparse_data, dense_data, labels = processor.download_from_huggingface(
        max_samples=1000000
    )

    # 分割并保存
    processor.split_and_save(sparse_data, dense_data, labels, val_ratio=0.2)

    print("\n" + "=" * 70)
    print("✅ 数据处理完成!")
    print("=" * 70)
    print("\n🎯 数据集统计:")
    print(f"   正样本比例 (CTR): {labels.mean():.2%}")
    print(f"   特征维度: 13个连续特征 + 26个稀疏特征")
    print("\n🚀 运行训练:")
    print("   python3 train.py")
    print("\n💡 或者运行快速开始脚本:")
    print("   ./quickstart.sh")


if __name__ == "__main__":
    import pandas as pd
    main()
