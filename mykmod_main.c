#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <mydev.h>

MODULE_DESCRIPTION("My kernel module - mykmod");
MODULE_AUTHOR("maruthisi.inukonda [at] gmail.com");
MODULE_LICENSE("GPL");

// Dynamically allocate major no
#define MYKMOD_MAX_DEVS 256
#define MYKMOD_DEV_MAJOR 0

static int mykmod_init_module(void);
static void mykmod_cleanup_module(void);

static int mykmod_open(struct inode *inode, struct file *filp);
static int mykmod_close(struct inode *inode, struct file *filp);
static int mykmod_mmap(struct file *filp, struct vm_area_struct *vma);

module_init(mykmod_init_module);
module_exit(mykmod_cleanup_module);

static struct file_operations mykmod_fops = {
	.owner  = THIS_MODULE,	/* owner (struct module *) */
	.open   = mykmod_open,	/* open */
	.release  = mykmod_close,     /* release */
	.mmap = mykmod_mmap,	/* mmap */
};

static void mykmod_vm_open(struct vm_area_struct *vma);
static void mykmod_vm_close(struct vm_area_struct *vma);
//static int mykmod_vm_fault(struct vm_fault *vmf);
static int mykmod_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

// Data-structure to keep per device info 
struct mykmod_dev_info {
	char *data;
	int *pageid;
};
// Data- structure for device table
struct mykmod_dev_info * Table[256] ;
int dev_count=0; // Initiallizing globally
// Data-structure to keep per VMA info 
struct vma_info {
	atomic_t count;
	struct mykmod_dev_info *dev_data;
	long npagefaults = 0; //initialized to zero
};

static const struct vm_operations_struct mykmod_vm_ops = {
	.open  = mykmod_vm_open,
	.close = mykmod_vm_close,
	.fault = mykmod_vm_fault
};

int mykmod_major;

static int mykmod_init_module(void)
{
	printk("mykmod loaded\n");
	printk("mykmod initialized at=%p\n", init_module);

	if ((mykmod_major = register_chrdev(MYKMOD_DEV_MAJOR,"mykmod",&mykmod_fops))<0) {
		printk(KERN_WARNING "Failed to register character device\n");
		return 1;
	}
	else {
		printk("register character device %d\n", mykmod_major);
	}
	// TODO initialize device table
	//initiaalized globally

	return 0;
}

static void mykmod_cleanup_module(void)
{
	printk("mykmod unloaded\n");
	unregister_chrdev(mykmod_major,"mykmod");
	// clearing device info structures from device table
	for(int k=0;k<dev_count;k++){
		kfree(Table[i]->Data);
		kfree(Table[i]);
	}
	return;
}

static int
mykmod_open(struct inode *inodep, struct file *filep)
{
	printk("mykmod_open: filep=%p f->private_data=%p "
		"inodep=%p i_private=%p i_rdev=%x maj:%d min:%d\n",
		filep, filep->private_data,
		inodep, inodep->i_private, inodep->i_rdev, MAJOR(inodep->i_rdev), MINOR(inodep->i_rdev));

	// Allocating memory for devinfo and store in device table and i_private.
	if (inodep->i_private == NULL) {
		struct mykmod_dev_info *info;
		info = kmalloc(sizeof(struct mykmod_dev_info), GFP_KERNEL);
		info->data = (char *)kzalloc(MYDEV_LEN, GFP_KERNEL);
		inodep->i_private = info;  // storing in i_private
		Table[dev_count] = info;  // Storing in device table
		dev_count+= 1; // Increasing the device count
	}

	// Storing device info in file's private_data aswell
	filep->private_data = inodep->i_private; 

	return 0;
}

static int
mykmod_close(struct inode *inodep, struct file *filep)
{
	printk("mykmod_close: inodep=%p filep=%p\n", inodep, filep);
	return 0;
}

static int mykmod_mmap(struct file *filp, struct vm_area_struct *vma)
{
	printk("mykmod_mmap: filp=%p vma=%p flags=%lx\n", filp, vma, vma->vm_flags);

	struct vma_info *dev_track; //Initializing structure with dev_info and npagefaults
	vma->vm_ops = &mykmod_vm_ops;	
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;	// 2 vma's flags
	dev_track = kmalloc(sizeof(struct vma_info), GFP_KERNEL);
	dev_track->dev_data = (struct mykmod_dev_info *)(filp->private_data);	// Indirectly alloting the stored inodep->i_private and saving in vm_private_data
	vma->vm_private_data = dev_track; 
	mykmod_vm_open(vma);

	return 0;
}

static void
mykmod_vm_open(struct vm_area_struct *vma)
{
	printk("mykmod_vm_open: vma=%p npagefaults:%lu\n", vma, ?);
}

static void
mykmod_vm_close(struct vm_area_struct *vma)
{
	printk("mykmod_vm_close: vma=%p npagefaults:%lu\n", vma, ?);
	npagefaults = 0;  // Resetting npagefaults to zero
}

static int
mykmod_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct vma_info *dev_track;	
	struct mykmod_dev_info *info;	// mapping data
	long offset;
	dev_track = (struct vma_info *)vma->vm_private_data;
	info = (struct mykmod_dev_info *)dev_track->dev_data;
	offset = (long)(vmf->pgoff << PAGE_SHIFT);	
	vmf->page = virt_to_page((long)info->data + offset);
	get_page(vmf->page);
	npagefaults+=1;		// pagefault count increased by 1
	printk("mykmod_vm_fault: vma=%p vmf=%p pgoff=%lu page=%p\n", vma, vmf, vmf->pgoff, vmf->page);
	// built virt->phys mappings

	return 0;
}


