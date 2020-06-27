# Size 种类

以width作为例子

类型 |         下限 |      建议    | 上限
----|--------------|--------------|----------
CSS | min-width    |         width| max-width
LUI | limited-width| fitting-width| max-width


CSS的属性会覆写LUI属性对应值，并且处于高优先级。即：
 - 如果指定了CSS属性，那么写入对应的LUI属性，再标记处于CSS状态
 - 如果没有指定CSS属性，那么LUI属性会按照相应的规则处理
 - 可以通过`style`或者直接的属性进行CSS属性设置

```cpp
case BKDR_MINWIDTH:
    // minwidth
    m_oStyle.limited.width = value.ToFloat();
    m_oStyle.overflow_sizestyled.min_width = true;
    break;
case BKDR_MINHEIGHT:
    // minheight
    m_oStyle.limited.height = value.ToFloat();
    m_oStyle.overflow_sizestyled.min_height = true;
    break;
```



一个最简单的例子就是图像`<image>`，建议宽度就是使用图片的宽度，下限宽度可以考虑`0`或者`1`。但是需要设置`flex`为有效值, 否则布局器依然会按照图片尺寸布局.

复杂点的例子就是箱型布局`<box>`。LongUI中，使用对应滚动条(这里就是水平滚动条)的`<box>`下限宽度会调整到`0`，建议宽度依然是有效子控件建议宽度(以及 `margin` 什么的，具体来说是 CSS 属性`box-sizing`控制的，XUL默认是`box-sizing: border-box`, LongUI中目前不支持动态切换`box-sizing`)累加.

而一旦使用CSS值，则以设置值为准。

正常控件大小应该介于建议值与最大值之间，特殊情况会处于最小值与建议值之间。而这种特殊情况`<box>`处理方式有点违背常理——`flex`越大的值反而最小，可以理解为以建议值作为起点，`flex`作为伸缩参数，越大的伸得越大，缩得也越大。


# 布局算法

LongUI中`<box>`布局算法如下, 为了方便说明先定义一些东西:
 - 有效布局子控件: `<box>`子控件中不是所有子控件都参与布局，最简单的，如果`visible`为`false` 或者是类似于滚动条之类的控件，是不参与布局的。而参与布局的子控件这里称为 有效布局子控件
 - 最低滚动尺寸: 如果内容区大小小于这个尺寸就需要显示滚动条或者滚动按钮
 - 建议尺寸: 这个上面以及说明了
 - 有效内容区大小: 所有的控件应该将子控件放在内容区，而`box`可能有滚动条或者滚动按钮，如果显示这些控件，有效内容区需要减去这些。
 
具体算法，以`<hbox>`为栗子:
 1. 确定有效内容区大小，自身建议宽度等预备数据
 2. 先将有效布局子控件中`flex`(`f`)累加起来作为`F`，`F`自然最小为`0`
 3. 有效内容区宽度 减去 建议宽度 作为`S`, 注意`S`可能为负数
 4. 遍历每个有效布局子控件，将控件临时宽度设置为建议宽度
 5. 令`U = S / F`. 当`F`为`0`时, 结束计算. (注: 如果`F`实现为浮点数时候注意浮点误差)
 6. 遍历每个有效布局子控件
 7. 将控件临时宽度 加上 `f * U`, 注意需要将宽度钳制到`min-width max-width`之间
 8. 让`S`减去 实际变化量
 9. `S`有变化，并且当宽度被钳制时, 让`F`减去`f`
 10. 如果`F`没有变化，也结束计算，否则回到步骤5。


算法复杂度: 
 - 极大概率: O(N)
 - 理论最差: O(N^2)

最后确定的临时宽度就是控件的最终宽度，而控件的另一维度，即高度，对齐方式由其他属性控制，与主维度宽度相比就简单多了，不再赘述。