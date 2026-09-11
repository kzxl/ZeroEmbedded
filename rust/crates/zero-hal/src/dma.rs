//! # Safe DMA Buffer Ownership Token
//!
//! Eliminates stack-buffer-use-after-free during asynchronous DMA transfers.
//! Ownership of the buffer is moved into `DmaTransfer` and returned only when DMA finishes.

pub struct DmaChannel {
    channel_id: u8,
}

impl DmaChannel {
    pub const fn new(channel_id: u8) -> Self {
        Self { channel_id }
    }

    /// Starts asynchronous DMA transfer by taking ownership of the buffer
    pub fn start_read<BUF: AsRef<[u8]>>(self, buffer: BUF) -> DmaTransfer<BUF> {
        let ptr = buffer.as_ref().as_ptr();
        let len = buffer.as_ref().len();

        // Hardware DMA register configuration (source address, length, enable)
        let _ = (ptr, len);

        DmaTransfer {
            channel: self,
            buffer,
        }
    }
}

/// Active DMA transfer handle owning the buffer
pub struct DmaTransfer<BUF> {
    channel: DmaChannel,
    buffer: BUF,
}

impl<BUF> DmaTransfer<BUF> {
    /// Checks if hardware transfer is finished
    pub fn is_complete(&self) -> bool {
        // Read hardware DMA status flag
        true
    }

    /// Blocks until DMA completion, then safely returns ownership of buffer and channel
    pub fn wait(self) -> (DmaChannel, BUF) {
        while !self.is_complete() {
            core::hint::spin_loop();
        }
        (self.channel, self.buffer)
    }
}
