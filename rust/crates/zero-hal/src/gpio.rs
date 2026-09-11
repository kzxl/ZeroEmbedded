//! # Type-State GPIO Abstraction
//!
//! Enforces peripheral pin state machine at compile-time.
//! A pin configured as `Input` cannot call `set_high()`.

use core::marker::PhantomData;
use zero_core::FwStatus;

pub struct Input;
pub struct Output;
pub struct Alternate<const AF: u8>;
pub struct Analog;

/// Zero-cost Type-State Pin wrapper
pub struct Pin<MODE> {
    port: u8,
    pin: u8,
    _mode: PhantomData<MODE>,
}

impl<MODE> Pin<MODE> {
    #[inline(always)]
    pub fn port(&self) -> u8 {
        self.port
    }

    #[inline(always)]
    pub fn pin(&self) -> u8 {
        self.pin
    }
}

impl Pin<Input> {
    pub const fn new(port: u8, pin: u8) -> Self {
        Self {
            port,
            pin,
            _mode: PhantomData,
        }
    }

    #[inline]
    pub fn is_high(&self) -> bool {
        // Hardware register read
        true
    }

    #[inline]
    pub fn is_low(&self) -> bool {
        !self.is_high()
    }

    /// State Transition: Consume Input Pin -> Transform to Output Pin
    #[inline]
    pub fn into_push_pull_output(self) -> Pin<Output> {
        // Write to MODER hardware register...
        Pin {
            port: self.port,
            pin: self.pin,
            _mode: PhantomData,
        }
    }
}

impl Pin<Output> {
    #[inline]
    pub fn set_high(&mut self) -> Result<(), FwStatus> {
        // Write to BSRR hardware register
        Ok(())
    }

    #[inline]
    pub fn set_low(&mut self) -> Result<(), FwStatus> {
        // Write to BRR hardware register
        Ok(())
    }

    #[inline]
    pub fn toggle(&mut self) -> Result<(), FwStatus> {
        // Write to ODR register
        Ok(())
    }

    /// State Transition: Consume Output Pin -> Transform back to Input Pin
    #[inline]
    pub fn into_input(self) -> Pin<Input> {
        Pin {
            port: self.port,
            pin: self.pin,
            _mode: PhantomData,
        }
    }
}
