mod pipe;
mod stdio;
mod inode;
mod mount;

use crate::mm::UserBuffer;
use alloc::sync::Arc; 

#[derive(Clone)]
pub enum FileClass {
    File (Arc<OSInode>),
    Abstr (Arc<dyn File + Send + Sync>),
}

pub trait File : Send + Sync {
    fn readable(&self) -> bool;
    fn writable(&self) -> bool;
    fn read(&self, buf: UserBuffer) -> usize;
    fn write(&self, buf: UserBuffer) -> usize;
}

pub use pipe::{Pipe, make_pipe};
pub use stdio::{Stdin, Stdout};
pub use inode::{OSInode, open, OpenFlags, list_apps, /*find_par_inode_id, */ch_dir, list_files,  DiskInodeType};