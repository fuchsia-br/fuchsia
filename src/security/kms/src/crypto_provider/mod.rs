// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cfg(test)]
pub mod mock_provider;
pub mod mundane_provider;
pub mod optee_provider;
pub mod software_provider;

use fidl_fuchsia_kms::{AsymmetricKeyAlgorithm, KeyProvider};
use std::error::Error;
use std::fmt;
use std::fmt::Debug;

/// The general error type returned by crypto provider.
#[derive(Clone, Debug)]
pub struct CryptoProviderError {
    // TODO(fxbug.dev/84729)
    #[allow(unused)]
    error_message: String,
}

impl fmt::Display for CryptoProviderError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl Error for CryptoProviderError {}

impl CryptoProviderError {
    pub fn new(error_message: &str) -> Self {
        CryptoProviderError { error_message: error_message.to_string() }
    }
}

/// A cryptography provider that could support cryptography operations.
pub trait CryptoProvider: Debug + Send + Sync {
    /// Return a list of supported algorithms.
    fn supported_asymmetric_algorithms(&self) -> &'static [AsymmetricKeyAlgorithm];

    /// Return the FIDL enum for this provider.
    fn get_name(&self) -> KeyProvider;

    /// Get a clone of the boxed trait object.
    fn box_clone(&self) -> Box<dyn CryptoProvider>;

    /// Generate an Asymmetric key pair. Return the generated key object.
    ///
    /// # Arguments:
    ///
    /// * `key_algorithm` - The algorithm for the key to be generated.
    /// * `key_name` - The name for the key in case the provider need to keep record.
    fn generate_asymmetric_key(
        &self,
        key_algorithm: AsymmetricKeyAlgorithm,
        key_name: &str,
    ) -> Result<Box<dyn AsymmetricProviderKey>, CryptoProviderError>;

    /// Import a key. Return the key data representing the imported key.
    ///
    /// # Arguments:
    ///
    /// * `key_data`- The key data to be imported by the provider.
    /// * `key_algorithm` - The algorithm for the key to be imported.
    /// * `key_name` - The name for the key in case the provider need to keep record.
    fn import_asymmetric_key(
        &self,
        key_data: &[u8],
        key_algorithm: AsymmetricKeyAlgorithm,
        key_name: &str,
    ) -> Result<Box<dyn AsymmetricProviderKey>, CryptoProviderError>;

    /// Check the correctness of key material and turn it into an AsymmetricProviderKey object.
    ///
    /// # Arguments:
    ///
    /// * `key_data`- The key data to be parsed by the provider.
    /// * `key_algorithm` - The algorithm for the key to be parsed.
    fn parse_asymmetric_key(
        &self,
        key_data: &[u8],
        key_algorithm: AsymmetricKeyAlgorithm,
    ) -> Result<Box<dyn AsymmetricProviderKey>, CryptoProviderError>;

    /// Generate a symmetric key for sealing. Return the generated key object.
    ///
    /// # Arguments:
    ///
    /// * `key_name` - The name for the key in case the provider need to keep record.
    fn generate_sealing_key(
        &self,
        key_name: &str,
    ) -> Result<Box<dyn SealingProviderKey>, CryptoProviderError>;

    /// Check the correctness of key material and turn it into an SealingProviderKey object.
    ///
    /// # Arguments:
    ///
    /// * `key_data`- The key data to be parsed by the provider.
    fn parse_sealing_key(
        &self,
        key_data: &[u8],
    ) -> Result<Box<dyn SealingProviderKey>, CryptoProviderError>;

    /// Calculate the size of the sealed data based on the original data size.
    ///
    /// # Arguments:
    ///
    /// * 'original_data_size' - The size of the original data.
    fn calculate_sealed_data_size(
        &self,
        original_data_size: u64,
    ) -> Result<u64, CryptoProviderError>;
}

impl Clone for Box<dyn CryptoProvider> {
    fn clone(&self) -> Self {
        self.box_clone()
    }
}

/// A key object in a generated by a crypto provider.
pub trait ProviderKey: Debug + Send {
    /// Delete a key.
    fn delete(&mut self) -> Result<(), CryptoProviderError>;
    /// Get the data for the key.
    fn get_key_data(&self) -> Vec<u8>;
    /// Get the crypto provider for the key.
    fn get_key_provider(&self) -> KeyProvider;
}

/// An asymmetric key object generated by a crypto provider.
pub trait AsymmetricProviderKey: ProviderKey {
    /// Sign a piece of data using asymmetric key. Return the signature.
    ///
    /// # Arguments:
    ///
    /// * `data` - The data to be signed.
    fn sign(&self, _data: &[u8]) -> Result<Vec<u8>, CryptoProviderError>;

    /// Get a DER encoded SubjectPublicKeyInfo structured public key data.
    fn get_der_public_key(&self) -> Result<Vec<u8>, CryptoProviderError>;

    /// Get the key algorithm.
    fn get_key_algorithm(&self) -> AsymmetricKeyAlgorithm;
}

/// An asymmetric key object generated by a crypto provider.
pub trait SealingProviderKey: ProviderKey {
    /// Encrypt data using symmetric key. Return the encrypted data.
    ///
    /// # Arguments:
    ///
    /// * `data` - The data to be encrypted.
    fn encrypt(&self, data: &[u8]) -> Result<Vec<u8>, CryptoProviderError>;

    /// Decrypt data using symmetric key. Return the original data.
    ///
    /// # Arguments:
    ///
    /// * `data` - The data to be decrypted.
    fn decrypt(&self, data: &[u8]) -> Result<Vec<u8>, CryptoProviderError>;
}
