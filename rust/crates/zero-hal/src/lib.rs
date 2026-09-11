#![no_std]

//! # zero-hal
//!
//! Generic type-safe hardware abstraction traits.

use zero_core::FwStatus;

/// Generic digital output pin
pub trait OutputPin {
    fn set_high(&mut self) -> Result<(), FwStatus>;
    fn set_low(&mut self) -> Result<(), FwStatus>;
    fn toggle(&mut self) -> Result<(), FwStatus>;
}

/// Generic digital input pin
pub trait InputPin {
    fn is_high(&self) -> Result<bool, FwStatus>;
    fn is_low(&self) -> Result<bool, FwStatus>;
}
