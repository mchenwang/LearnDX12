# day day up

## root signature

![](./image/RS.png)

1. 访问根签名中的Constant，效率最高，耗时1个单位；
2. 其次，可以直接将CBV/SRV/UAV绑定到根签名上，需要通过view，访问heap中的数据，耗时2个单位；
3. 可以将一堆view放到一个descriptor table中，通过descriptor range管理table，shader通过先访问table，再通过view访问heap中的数据，耗时3个单位；虚线框出来的应该是抽象出来的一层，实际上应该不需要额外访问开销。