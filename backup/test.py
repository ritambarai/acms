# ml/Neural/fridge_new/fridge_model.py

import keras
from keras import layers
import tensorflow as tf

from keras.saving import get_custom_objects

# -------------------------------------------------
# Custom Initializer
# -------------------------------------------------
@keras.saving.register_keras_serializable()
class TruncatedNormalLike(keras.initializers.Initializer):
    def __init__(self, mean=0.0, stddev=0.02):
        self.mean = mean
        self.stddev = stddev

    def __call__(self, shape, dtype=None):
        return tf.random.truncated_normal(
            shape, mean=self.mean, stddev=self.stddev, dtype=dtype
        )

    def get_config(self):
        return {"mean": self.mean, "stddev": self.stddev}

@keras.saving.register_keras_serializable()
class L2Pooling1D(layers.Layer):
    def __init__(self, pool_size=2, strides=2, padding="valid", **kwargs):
        super().__init__(**kwargs)
        self.pool_size = pool_size
        self.strides = strides
        self.padding = padding

    def call(self, inputs):
        squared = tf.square(inputs)
        pooled = tf.nn.avg_pool1d(
            squared,
            ksize=self.pool_size,
            strides=self.strides,
            padding=self.padding.upper(),
        )
        return tf.sqrt(pooled + 1e-12)

    def get_config(self):
        return {
            "pool_size": self.pool_size,
            "strides": self.strides,
            "padding": self.padding,
        }


# -------------------------------------------------
# Positional Embedding
# -------------------------------------------------
@keras.saving.register_keras_serializable()
class PositionalEmbedding(layers.Layer):
    def __init__(self, max_len, d_model, **kwargs):
        super().__init__(**kwargs)
        self.max_len = max_len
        self.d_model = d_model
        self.emb = layers.Embedding(input_dim=max_len, output_dim=d_model)

    def call(self, inputs):
        seq_len = tf.shape(inputs)[1]
        positions = tf.range(start=0, limit=seq_len, delta=1)
        pos_embed = self.emb(positions)
        pos_embed = tf.expand_dims(pos_embed, 0)
        return pos_embed

    def get_config(self):
        return {
            "max_len": self.max_len,
            "d_model": self.d_model,
        }


# -------------------------------------------------
# Transformer Block
# -------------------------------------------------
@keras.saving.register_keras_serializable()
class TransformerBlock(layers.Layer):
    def __init__(self, hidden, num_heads, ff_dim, dropout, **kwargs):
        super().__init__(**kwargs)
        self.att = layers.MultiHeadAttention(
            num_heads=num_heads, key_dim=hidden
        )
        self.ffn = keras.Sequential(
            [
                layers.Dense(ff_dim, activation=tf.keras.activations.gelu),
                layers.Dense(hidden),
            ]
        )
        self.layernorm1 = layers.LayerNormalization(epsilon=1e-6)
        self.layernorm2 = layers.LayerNormalization(epsilon=1e-6)
        self.dropout = layers.Dropout(dropout)

    def call(self, inputs, training=False):
        attn_output = self.att(inputs, inputs)
        out1 = self.layernorm1(
            inputs + self.dropout(attn_output, training=training)
        )
        ffn_output = self.ffn(out1)
        out2 = self.layernorm2(
            out1 + self.dropout(ffn_output, training=training)
        )
        return out2


# -------------------------------------------------
# Custom Loss (required ONLY for loading)
# -------------------------------------------------
@keras.saving.register_keras_serializable()
def fridge_loss(y_true, y_pred):
    ap_true = y_true[:, :, 0]
    rp_true = y_true[:, :, 1]
    ap_pred = y_pred[:, :, 0]
    rp_pred = y_pred[:, :, 1]
    mae_ap = tf.reduce_mean(tf.abs(ap_pred - ap_true))
    mae_rp = tf.reduce_mean(tf.abs(rp_pred - rp_true))
    return 0.7 * mae_ap + 0.3 * mae_rp

# -------------------------------------------------
# FORCE legacy registration (TF-Keras compatibility)
# -------------------------------------------------
get_custom_objects()["TruncatedNormalLike"] = TruncatedNormalLike
get_custom_objects()["L2Pooling1D"] = L2Pooling1D
get_custom_objects()["PositionalEmbedding"] = PositionalEmbedding
get_custom_objects()["TransformerBlock"] = TransformerBlock
get_custom_objects()["fridge_loss"] = fridge_loss

# -------------------------------------------------
# Load FULL .keras model
# -------------------------------------------------
def load_fridge_model(model_path: str):
    return keras.models.load_model(
        model_path,
        compile=False,
    )
